# Consensus

* This here is a Chrome-enabled version of consensus. To install, you need to:
<ol>
<li> make consensus (see below)
<li> setup local WebServer on your system - cf. https://www.maketecheasier.com/setup-local-web-server-all-platforms/
<li> configure Apache to permit CGI - cf. https://httpd.apache.org/docs/current/howto/cgi.html
<li> copy the consensus executable under /Library/WebServer/CGI-Executables
<li> copy the entire contents of the distribution's 'Documents' directory under /Library/WebServer/Documents
</ol>
* To run, you need to
<ol>
<li> launch consensus in operator mode, e.g. from the distribution's directory
```
    ./consensus -moperator
````
<li> enter the URL 'localhost/cgi-bin/consensus' in Chrome
</ol>

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
