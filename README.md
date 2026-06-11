This is my final project for ECE531: Introduction to the Internet of Things.

The code will be a work in progress during the entire semester, and hopefully be working!

From the professor, the guidelines are as follows:
"In this assignment, you will create an IoT client that communicates with the cloud. The IoT client is a smart thermostat that exchanges commands with a remote server over HTTP/HTTPS. You are responsible for the application client, and I have provided a thermocouple simulation that writes information to two files in /tmp, /tmp/temp (which contains the temperature) and /tmp/status (which indicates whether the heater is on or off). You will build this client as well as the server that stores programs and other information.
The thermocouple simulation is here, on github: https://github.com/cclamb/thermocouple
This is, essentially, beta software! You are the first users. When you find issues, please open an issue on github here: https://github.com/cclamb/thermocouple/issues
As issues come in, and I fix them, I'll let you know via email that you want to acquire new code.
This is a major part of your grade and I want you to do well! I've tailored the assignments so you should have learned what you need to know to build the client, and you're currently learning how to build the server. Your application should run as a daemon and start on system startup; you did this with your daemon assignments. You've used HTTP and major verbs (e.g. GET, PUT, POST, and DELETE) via libcurl to communicate with websites, so you should be ready to communicate with a REST interface.
For additional details on the client, see the first two videos in Module 3.

Here's the rubric:
110%: All the below and you use HTTPS to communicate with the cloud!
100%: Everything works as described in the project presentations in module 3.
80%: The program runs, but doesn't pay attention to the status file, or doesn't handle programs correctly.
60%: The program runs, but doesn't start up on boot.
0%: Your client device doesn't boot.

You need to turn in an archive containing all elements of your VM, and it must be configured to attach to your cloud code. Name the archive project.tgz, creating the archive as you did for the attack surface assignment. This archive must contain everything needed to run the client including a script (like qemu-versatile.sh) that will run your image (name this script project.sh). I must be able to extract the archive, execute the script, and your VM should run. I should see your program contacting the server and communicating. Your code should start up automatically when the image boots up, so you need to configure the appropriate initialization files so this happens. Also include a text file containing credentials for the image (name this file credentials.txt) - I need the root password of the image as well as the usernames and passwords any user accounts you're using.
So archive contains: project.sh, credentials.txt, and all other dependencies for your VM. I will run project.sh, and your VM image must boot and run your application!
You can submit as many times as you want, through the end of the term. I highly, highly suggest you get this finished as soon as possible and turned in, so if anything doesn't work, you can fix and resubmit. If you put this off to the end of the term, you won't have a resubmission option."
