# Reproduce the results of TCP BBR

This repository contains the source code related to the TCP BBR paper submitted to WNS3-2018.

## Steps to reproduce

* Step 1: Clone this repo:

``git clone https://github.com/Vivek-anand-jain/Reproduce-TCP-BBR-in-ns-3``

* Step 2: Configure and build the cloned repo:

```
./waf configure
./waf
```

* Step 3: Run TCP BBR example available under ``scratch`` directory:

``./waf --run scratch/tcp-bbr-exam``

Result will be generated under ``results/`` directory.
