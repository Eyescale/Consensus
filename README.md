# Consensus

* This here is a Chrome-enabled version of consensus.
* To install, you need to:
<div style="list-style-type:decimal">
    1. make consensus (see below)
    2. setup local WebServer on your system - cf.  
       https://www.maketecheasier.com/setup-local-web-server-all-platforms/
    3. configure Apache to permit CGI - cf.  
       https://httpd.apache.org/docs/current/howto/cgi.html
    4. copy the consensus executable under /Library/WebServer/CGI-Executables
    5. copy the entire contents of the distribution's 'Documents' directory under /Library/WebServer/Documents
</div>
* To run, you need to
<div style="list-style-type:decimal">
    1. launch consensus in operator mode, e.g. from the distribution's directory type './consensus -moperator'
    2. enter the URL 'localhost/cgi-bin/consensus' in Chrome
</div>

----

* To compile, you need to create a .ofiles directory
* To run, just launch ./consensus
* after the prompt, you can type e.g. the command
```
    :< file:test/full
```
* that should get you started!
* documentation is under way...


[![Build Status](https://travis-ci.org/Eyescale/Consensus.svg?branch=master)](https://travis-ci.org/Eyescale/Consensus)
