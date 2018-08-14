# Consensus

* Under Pilot/ you will find the Chrome-enabled, Pilot version of consensus.
* To install, you need to:
    * make consensus (see below)
    * setup local WebServer on your system - cf.  
       https://www.maketecheasier.com/setup-local-web-server-all-platforms/
    * configure Apache to permit CGI - cf.  
       https://httpd.apache.org/docs/current/howto/cgi.html
    * copy the consensus executable under /Library/WebServer/CGI-Executables
    * copy the entire contents of the Pilot's 'Documents' sub-directory under /Library/WebServer/Documents
* To run, you need to
    * launch consensus in operator mode, e.g. from the distribution's directory type
    ```
        ./consensus -moperator
    ```
    * enter the URL 'localhost/cgi-bin/consensus' in Chrome

----

* To compile, you need to create a .ofiles directory
```
    cd Pilot
    mkdir .ofiles
    make
```
* To run, just launch ./consensus
* after the prompt, you can type e.g. the command
```
    :< file:test/full
```
* that should get you started!
* documentation is under way...


[![Build Status](https://travis-ci.org/Eyescale/Consensus.svg?branch=master)](https://travis-ci.org/Eyescale/Consensus)
