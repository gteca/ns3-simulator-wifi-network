# ns3-simulator-wifi-network

This repository contains a implementation of IEEE 802.11 Wireless Local Area Network (WLAN) using [ns-3 network simulator](https://www.nsnam.org/).<br />
### Features 
- IEEE 802.11 ax
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
## Clone and run the wifi-network from this repository
Once you have installed and build ns-3 simulator repository, in order to run thw wifi-network of this repository:
- ##### Clone this repository via HTTPS
```console
foo@bar:~$ mkdir ns-3-wifi-network
foo@bar:~$ cd ns-3-wifi-network
foo@bar:~$ git clone https://github.com/gteca/ns3-simulator-wifi-network.git
foo@bar:~$ cd ns3-simulator-wifi-network
```
- ##### Create a symbolic link inside the installed ns-3 simulator 
You will work inside the directory where the ns3-simulator-wifi-network git repository is placed, in order to build and run the wifi-network you should tell ns-3 simulator where is the file to be compiled and built, for this purpose create a symbolic link to the target file inside the network simulator.

```console
foo@bar:~$ cd /home/your_user_name/ns-3-simulator/ns-allinone-3.35/ns-3.35/scratch
foo@bar:~$ ln -s /home/your_user_name/ns-3-wifi-network/ns3-simulator-wifi-network/wifi-network.cc wifi-infrastructure.cc
```
- ##### Build and run the wifi network
```console
foo@bar:~$ cd ..
foo@bar:~$ ./waf --run scratch/wifi-infrastructure.cc
foo@bar:~$ ln -s /home/your_user_name/ns-3-wifi-network/ns3-simulator-wifi-network/wifi-network.cc wifi-network.cc
```
You shall see the output as follows:
```console
Flow 1 From (Station) with IP : 10.1.1.1 To (AP) with address : 10.1.1.3
  Tx Packets: 5494
  Tx Bytes:   5779688
  TxOffered:  4.62375 Mbps
  Rx Packets: 5493
  Rx Bytes:   5778636

 Flow 2 From (Station) with IP : 10.1.1.2 To (AP) with address : 10.1.1.3
  Tx Packets: 5494
  Tx Bytes:   5779688
  TxOffered:  4.62375 Mbps
  Rx Packets: 5491  
  Rx Bytes:   5776532
Offered = 5e+06
Throughput = 9.24413

```






