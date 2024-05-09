# Ledbat++ implementation in ns-3

This project implements the TCP Ledbat++ congestion control algorithm over the ns-3 simulator (using version 3.37).

The Ledbat++ model has been implemented according to https://datatracker.ietf.org/doc/html/draft-irtf-iccrg-ledbat-plus-plus-01 and is built upon the tcp-ledbat implementation as provided by the ns-3 codebase.

To use the model 

1) Copy the _tcp-ledbatpp.cc_ and _tcp-ledbatpp.h_ files into the _'src/internet/model'_ directory.
2) Edit the _'CMakeLists.txt'_ file found under the _'src/internet'_ directory to include the _tcp-ledbatpp.cc_ and _tcp-ledbatpp.h_ files in the source files and header files list, respectively, like so:
```
    set(source_files
    ...
    model/tcp-bbr.cc
    model/tcp-bic.cc
    model/tcp-congestion-ops.cc
    model/tcp-cubic.cc
    model/tcp-dctcp.cc
    model/tcp-header.cc
    model/tcp-highspeed.cc
    model/tcp-htcp.cc
    model/tcp-hybla.cc
    model/tcp-illinois.cc
    model/tcp-l4-protocol.cc
    model/tcp-ledbat.cc
    model/tcp-ledbatpp.cc
    model/tcp-linux-reno.cc
    model/tcp-lp.cc
    ...
)
```

```set(header_files
    ${header_files}
    ...
    model/tcp-bbr.h
    model/tcp-bic.h
    model/tcp-congestion-ops.h
    model/tcp-cubic.h
    model/tcp-dctcp.h
    model/tcp-header.h
    model/tcp-highspeed.h
    model/tcp-htcp.h
    model/tcp-hybla.h
    model/tcp-illinois.h
    model/tcp-l4-protocol.h
    model/tcp-ledbat.h
    model/tcp-ledbatpp.h
    model/tcp-linux-reno.h
    model/tcp-lp.h
    ...
)
```

   
4) (Re-)build ns-3.
5) You may use the tcp-variants-comparison example (found under the _/examples/tcp_ directory) to experiment with Ledbat++ by setting 'TcpLedbatpp' as the transport protocol:
   ```
   std::string transport_prot = "TcpLedbatpp";
   ```

   You may get logging info by enabling NS_LOG before running the example, e.g.:
   ```
   export 'NS_LOG=TcpLedbatpp=level_info|prefix_time|prefix_func'
    ```

## References
<a id="1">[1]</a> 
Kapetanidou, Ioanna Angeliki, et al. "Experimenting with Ledbat++: Fairness, Flow Scalability and LBE Compliance." 

_Preprint_: https://www.researchgate.net/publication/380096217_Experimenting_with_Ledbat_Fairness_Flow_Scalability_and_LBE_Compliance
  
