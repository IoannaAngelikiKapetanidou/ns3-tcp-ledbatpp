This folder contains the scripts used for the experiments in paper _'Experimenting with Ledbat++: Fairness, Flow Scalability and LBE Compliance'_, to be presented in IFIP Networking 2024 conference.

The scripts have been used in the experiments described in the paper, as follows:

- _tcp-variants-comparison.cc_: Used in Experiment 1. Based on the homonymous script provided in ns-3.
- _tcp-variants-comparison-with-reverse-flows.cc_: Used in Experiment 2, Scenario 1. Extends _tcp-variants-comparison.cc_.
- _cross-traffic-multi-bottleneck.cc_: Used in Experiment 2, Scenario 2. Extends _tcp-variants-comparison-with-reverse-flows.cc_.


Currently, the scripts provide measurements of the total throughput up to the simulation time point. To calculate average throughput over a measurement window, equations in _PrintThroughput_ and _PrintFairness_ functions need to be adjusted, and _rxS1R1Bytes_ and _rxS2R2Bytes_ buffers have to be initialized periodically.
