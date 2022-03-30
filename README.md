# ns3-simulator-wifi-network

This repository contains a implementation of IEEE 802.11 Wireless Local Area Network (WLAN) using [ns-3 network simulator](https://www.nsnam.org/).<br />
### Features 
- IEEE 802.11 ac
- 1 AP and N Stations
- UDP Server and Client echo application
- BBS (infrastructure) topology

### Donwloading ns-3 simulator 
There are many ways to download ns-3 simulator and can be found [here](https://www.nsnam.org/docs/release/3.35/tutorial/html/getting-started.html#downloading-a-release-of-ns-3-as-a-source-archive). For this repository the fastest way was by downloading the release of ns-3 as a source archive (The version is ns-3.35).
```console
foo@bar:~$ cd
foo@bar:~$ mkdir ns-3-simulator
foo@bar:~$ cd ns-3-simulator
foo@bar:~$ wget https://www.nsnam.org/release/ns-allinone-3.35.tar.bz2
foo@bar:~$ tar xjf ns-allinone-3.35.tar.bz2
```
### Building ns-3
There are few ways to build ns-3 simulator and can be found [here](https://www.nsnam.org/docs/release/3.35/tutorial/html/getting-started.html#building-ns-3). For this repository a built using [Waf](https://www.nsnam.org/docs/release/3.35/tutorial/html/getting-started.html#building-with-waf). You probably don't need to run  ```./waf clean``` if you are building for the first time.

```console
foo@bar:~$ cd ns-allinone-3.35/ns-3.35
foo@bar:~$ ./waf clean 
foo@bar:~$ ./waf configure --build-profile=optimized --enable-examples --enable-tests
```
The script runs a check for the functionalities installed to suppor the full functionality of the simulator, the most important thing is the last output message that will confirm that the configuration finished successfully. You can switch the build profile to debug to allow to print debug messages during the simulation when enabled
```console
foo@bar:~$ ./waf clean
foo@bar:~$ ./waf configure --build-profile=debug --enable-examples --enable-tests
```
To build the entire ns-3 repository type:
```console
foo@bar:~$ ./waf 
```
[!] The building take a long
ou can run the unit tests of the ns-3 distribution by running the ./test.py script:
```console
foo@bar:~$ ./test.py
```
### Running a Script
To run a already written script to confirm that the installation 1finished successful, run the hello-simulator using Waf, and you shall se the output:
```console
foo@bar:~$ ./waf --run hello-simulator
foo@bar:~$ Hello Simulator
```
If the output is not displayed, change the build-profile to debug and rebuild the project:
```console
foo@bar:~$ ./waf configure --build-profile=debug --enable-examples --enable-tests
foo@bar:~$ ./waf
```
***
### Clone and run the wifi-network from this repository

