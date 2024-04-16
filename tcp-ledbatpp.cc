#include "tcp-ledbatpp.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include <math.h>       

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpLedbatpp");
NS_OBJECT_ENSURE_REGISTERED (TcpLedbatpp);

TypeId
TcpLedbatpp::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpLedbatpp")
    .SetParent<TcpNewReno> ()
    .AddConstructor<TcpLedbatpp> ()
    .SetGroupName ("Internet")
    .AddAttribute ("TargetDelay",
                   "Targeted Queue Delay",
                   TimeValue (MilliSeconds (60)),
                   MakeTimeAccessor (&TcpLedbatpp::m_target),
                   MakeTimeChecker ())
    .AddAttribute ("BaseHistoryLen",
                   "Number of Base delay samples",
                   UintegerValue (10),
                   MakeUintegerAccessor (&TcpLedbatpp::m_baseHistoLen),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("NoiseFilterLen",
                   "Number of Current delay samples",
                   UintegerValue (4),
                   MakeUintegerAccessor (&TcpLedbatpp::m_noiseFilterLen),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Gain",
                   "Offset Gain",
                   DoubleValue (1.0),
                   MakeDoubleAccessor (&TcpLedbatpp::m_gain),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("SSParam",
                   "Possibility of Slow Start",
                   EnumValue (DO_SLOWSTART),
                   MakeEnumAccessor (&TcpLedbatpp::SetDoSs),
                   MakeEnumChecker (DO_SLOWSTART, "yes",
                                    DO_NOT_SLOWSTART, "no"))
    .AddAttribute ("MinCwnd",
                   "Minimum cWnd for Ledbat",
                   UintegerValue (2),
                   MakeUintegerAccessor (&TcpLedbatpp::m_minCwnd),
                   MakeUintegerChecker<uint32_t> ())
  ;
  return tid;
}

void TcpLedbatpp::SetDoSs (SlowStartType doSS)
{
  NS_LOG_FUNCTION (this << doSS);
  m_doSs = doSS;
  if (m_doSs)
    {
      m_flag |= LEDBAT_CAN_SS;
    }
  else
    {
      m_flag &= ~LEDBAT_CAN_SS;
    }
}

TcpLedbatpp::TcpLedbatpp (void)
  : TcpNewReno ()
{
  NS_LOG_FUNCTION (this);
  m_target = MilliSeconds (60);
  m_gain = 1;
  m_doSs = DO_SLOWSTART;
  m_baseHistoLen = 10;
  m_noiseFilterLen = 4;
  InitCircBuf (m_baseHistory);
  InitCircBuf (m_noiseFilter);
  m_lastRollover = 0;
  m_sndCwndCnt = 0;
  m_flag = LEDBAT_CAN_SS;
  m_minCwnd = 2;
  constant = 1; //constant used in the m_gain formula, set to 1 as recommended by draft-irtf-iccrg-ledbat-plus-plus-01
  slowdown_end  = 0; //ending time of slowdown
  slowdown_init = 0; //starting time of slowdown
  slowdown_period = 0; //slowdown period
  slowdown_init_flag = false; //flag to check whether computing the next slowdown period has started
  slowdown_end_flag = false; //flag to check whether the next slowdown period slowdown has been
  init_slowstart = false; //flag to ensure that exit slow start on excessive delay is applied only during the initial slow start
  cwnd_freeze = -1; //used to keep cwnd frozen for 2 RTTs during periodic slowdowns
  frozen_cwnd = false; //flag to check whether we are in keeping cwnd frozen period
 
};



void TcpLedbatpp::InitCircBuf (struct OwdCircBuf &buffer)
{
  NS_LOG_FUNCTION (this);
  buffer.buffer.clear ();
  buffer.min = 0;
}

TcpLedbatpp::TcpLedbatpp (const TcpLedbatpp& sock)
  : TcpNewReno (sock)
{
  NS_LOG_FUNCTION (this);
  m_target = sock.m_target;
  m_gain = sock.m_gain;
  m_doSs = sock.m_doSs;
  m_baseHistoLen = sock.m_baseHistoLen;
  m_noiseFilterLen = sock.m_noiseFilterLen;
  m_baseHistory = sock.m_baseHistory;
  m_noiseFilter = sock.m_noiseFilter;
  m_lastRollover = sock.m_lastRollover;
  m_sndCwndCnt = sock.m_sndCwndCnt;
  m_flag = sock.m_flag;
  m_minCwnd = sock.m_minCwnd;

}

TcpLedbatpp::~TcpLedbatpp (void)
{
  NS_LOG_FUNCTION (this);
}

Ptr<TcpCongestionOps>
TcpLedbatpp::Fork (void)
{
  return CopyObject<TcpLedbatpp> (this);
}

std::string
TcpLedbatpp::GetName () const
{
  return "TcpLedbat++";
}

uint32_t TcpLedbatpp::MinCircBuf (struct OwdCircBuf &b)
{
  NS_LOG_FUNCTION_NOARGS ();
  if (b.buffer.size () == 0)
    {
      return ~0U;
    }
  else
    {
      return b.buffer[b.min];
    }
}

uint32_t TcpLedbatpp::CurrentDelay (FilterFunction filter)
{
  NS_LOG_FUNCTION (this);
  return filter (m_noiseFilter);
}

uint32_t TcpLedbatpp::BaseDelay ()
{
  NS_LOG_FUNCTION (this);
  return MinCircBuf (m_baseHistory);
}


//Modified SlowStart mechanism
uint32_t
TcpLedbatpp::Slowstart (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked);

  NS_LOG_INFO ("Entered SlowStart with cwnd " << tcb->m_cWnd << " ssthresh " << tcb->m_ssThresh);
 
  if (segmentsAcked >= 1)
    {
      tcb->m_cWnd += tcb->m_segmentSize*m_gain;
      NS_LOG_INFO ("In SlowStart, updated to cwnd " << tcb->m_cWnd << " ssthresh " << tcb->m_ssThresh);
      return segmentsAcked - 1;
    }
 
  return 0;
}

void TcpLedbatpp::IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked);
  if (tcb->m_cWnd.Get () <= tcb->m_segmentSize)
    {
      m_flag |= LEDBAT_CAN_SS;
    }
  

  int64_t queue_delay;
  uint64_t current_delay = CurrentDelay (&TcpLedbatpp::MinCircBuf);
  uint64_t base_delay = BaseDelay ();
  
  if (current_delay > base_delay)
    {
      queue_delay = static_cast<int64_t> (current_delay - base_delay);
    }
  else
    {
      queue_delay = static_cast<int64_t> (base_delay - current_delay);
    }


  //Scheduling of periodic Slowdowns
    if (tcb->m_cWnd>=tcb->m_ssThresh && slowdown_end_flag==false && Simulator::Now ().GetSeconds ()>slowdown_init){
      slowdown_end = Simulator::Now ().GetSeconds ();
      slowdown_period=slowdown_end -slowdown_init;
      slowdown_end_flag=true;
      NS_LOG_INFO ("Scheduling slowdown after 9*" << slowdown_period << " = " << 9*slowdown_period << " seconds.");
      Simulator::Schedule (Seconds (9*slowdown_period),&TcpLedbatpp::Slowdown,this,tcb,segmentsAcked);                    
                      
  } 

  
  if (m_doSs == DO_SLOWSTART && tcb->m_cWnd <= tcb->m_ssThresh && (m_flag & LEDBAT_CAN_SS) && init_slowstart==1)
    {      
     if (frozen_cwnd && Simulator::Now().GetSeconds() > cwnd_freeze){
     	Slowstart(tcb, segmentsAcked);
	cwnd_freeze = -1;
	frozen_cwnd = false;
      }
      else if (!frozen_cwnd) CongestionAvoidance (tcb, segmentsAcked);
      
    }  
 //Ιnitial Slowstart: if queuing delay > 3/4ths of target delay, exit slow start and immediately switch to congestion avoidance phase
  else if (m_doSs == DO_SLOWSTART && tcb->m_cWnd <= tcb->m_ssThresh && (m_flag & LEDBAT_CAN_SS) && init_slowstart==0 && queue_delay <= 3/4 * (m_target.GetMilliSeconds ())) 
    {
      Slowstart(tcb, segmentsAcked);          
    }

  else
    {
      m_flag &= ~LEDBAT_CAN_SS;
      
	if (m_doSs == DO_SLOWSTART && tcb->m_cWnd <= tcb->m_ssThresh && (m_flag & LEDBAT_CAN_SS) && init_slowstart==0 && queue_delay > 3/4 * (m_target.GetMilliSeconds ())) {
	 NS_LOG_INFO ("Exited initial slowstart due to excessive delay");
      }

      if (!slowdown_init_flag) {
	const Time& rtt = tcb->m_lastRtt;
	NS_LOG_INFO ("Scheduling initial slowdown after " << 2*rtt.GetSeconds() << " seconds.");
	Simulator::Schedule (Seconds (2*rtt.GetSeconds()),&TcpLedbatpp::Slowdown,this,tcb,segmentsAcked);  //enter initial slowdown 2 RTT after the initial slow start completes
	slowdown_init=Simulator::Now().GetSeconds() + 2*rtt.GetSeconds();
	NS_LOG_INFO ("slowdown_init = " << slowdown_init << " seconds.");
        slowdown_init_flag=1;	
       }

      init_slowstart=1;
      CongestionAvoidance (tcb, segmentsAcked);
    }
}


//Modified CongestionAvoidance that combines additive increases and multiplicative decreases
void TcpLedbatpp::CongestionAvoidance (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)

{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked);
  if ((m_flag & LEDBAT_VALID_OWD) == 0)
    {
      TcpNewReno::CongestionAvoidance (tcb, segmentsAcked); //letting it fall to TCP behaviour if no timestamps
      return;
    }
  
  int64_t queue_delay;  
  uint32_t cwnd = (tcb->m_cWnd.Get ());
  uint64_t current_delay = CurrentDelay (&TcpLedbatpp::MinCircBuf);
  uint64_t base_delay = BaseDelay ();
  
  m_gain= 1 / (std::min (16, static_cast<int>(ceil(2*m_target.GetSeconds()/base_delay))));
  
  
  if (current_delay > base_delay)
    {
      queue_delay = static_cast<int64_t> (current_delay - base_delay);
     
    }
  else
    {
      queue_delay = static_cast<int64_t> (base_delay - current_delay);
     
    }


  if (queue_delay <= m_target.GetMilliSeconds())
	  cwnd+=m_gain;
  else 
	  cwnd+= std::max(static_cast<uint32_t>(m_gain-constant*cwnd*(queue_delay/m_target.GetMilliSeconds()-1)),-cwnd/2);

  tcb->m_cWnd = cwnd;
 
  if (tcb->m_cWnd <= tcb->m_ssThresh)
    {
      tcb->m_ssThresh = tcb->m_cWnd - 1;
    }

  NS_LOG_INFO ("In Congestion Avoidance, updated to cwnd " << tcb->m_cWnd << " ssthresh " << tcb->m_ssThresh);


}


//TcpLedbat++ Slowdown Mechanism
void TcpLedbatpp::Slowdown (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked){

 const Time& rtt = tcb->m_lastRtt;
   
  tcb->m_ssThresh = tcb->m_cWnd;	
  tcb->m_cWnd=2;	 
  cwnd_freeze = Simulator::Now().GetSeconds() + 2*rtt.GetSeconds();
  NS_LOG_INFO ("Keep Cwnd frozen to 2 packets for 2 RTTs=" << 2*rtt.GetSeconds() << " seconds.");
  NS_LOG_INFO ("Exit freeze period at " << cwnd_freeze << " seconds.");
  frozen_cwnd = true;
  //Simulator::Schedule (Seconds (2*rtt.GetSeconds()),&TcpLedbatpp::Slowstart,this,tcb,segmentsAcked); 
 /*if (slowdown_end_flag) {
	NS_LOG_INFO ("Next slowdown after " << 9*slowdown_period << " seconds.");
	Simulator::Schedule (Seconds (9*slowdown_period),&TcpLedbatpp::Slowdown,this,tcb,segmentsAcked); 
  }  */
  
  slowdown_end_flag = false;
  slowdown_init = Simulator::Now().GetSeconds();

}




void TcpLedbatpp::AddDelay (struct OwdCircBuf &cb, uint32_t owd, uint32_t maxlen)
{
  NS_LOG_FUNCTION (this << owd << maxlen << cb.buffer.size ());
  if (cb.buffer.size () == 0)
    {
      NS_LOG_LOGIC ("First Value for queue");
      cb.buffer.push_back (owd);
      cb.min = 0;
      return;
    }
  cb.buffer.push_back (owd);
  if (cb.buffer[cb.min] > owd)
    {
      cb.min = static_cast<uint32_t> (cb.buffer.size () - 1);
    }
  if (cb.buffer.size () >= maxlen)
    {
      NS_LOG_LOGIC ("Queue full " << maxlen);
      cb.buffer.erase (cb.buffer.begin ());
      cb.min = 0;
      NS_LOG_LOGIC ("Current min element " << cb.buffer[cb.min]);
      for (uint32_t i = 1; i < maxlen - 1; i++)
        {
          if (cb.buffer[i] < cb.buffer[cb.min])
            {
              cb.min = i;
            }
        }
    }
}

void TcpLedbatpp::UpdateBaseDelay (uint32_t owd)
{
  NS_LOG_FUNCTION (this << owd );
  if (m_baseHistory.buffer.size () == 0)
    {
      AddDelay (m_baseHistory, owd, m_baseHistoLen);
      return;
    }
  uint64_t timestamp = static_cast<uint64_t> (Simulator::Now ().GetSeconds ());

  if (timestamp - m_lastRollover > 60)
    {
      m_lastRollover = timestamp;
      AddDelay (m_baseHistory, owd, m_baseHistoLen);
    }
  else
    {
      uint32_t last = static_cast<uint32_t> (m_baseHistory.buffer.size () - 1);
      if (owd < m_baseHistory.buffer[last])
        {
          m_baseHistory.buffer[last] = owd;
          if (owd < m_baseHistory.buffer[m_baseHistory.min])
            {
              m_baseHistory.min = last;
            }
        }
    }
}

void TcpLedbatpp::PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked,
                           const Time& rtt)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked << rtt);
  if (tcb->m_rcvTimestampValue == 0 || tcb->m_rcvTimestampEchoReply == 0)
    {
      m_flag &= ~LEDBAT_VALID_OWD;
    }
  else
    {
      m_flag |= LEDBAT_VALID_OWD;
    }
  if (rtt.IsPositive ())
    {
      AddDelay (m_noiseFilter, rtt.GetMilliSeconds(), m_noiseFilterLen);
      UpdateBaseDelay (rtt.GetMilliSeconds());
    }
}

} // namespace ns3
