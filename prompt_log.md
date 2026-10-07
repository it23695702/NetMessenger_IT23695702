# AI Prompt Log – IE3010 NetMessenger

**Registration Number:** IT23695702

## Purpose of Using AI

I used AI mainly as a learning and support tool while doing this implementation. I used it to understand some socket programming concepts, clarify errors, and understand how some C functions work.

The implementation was personalised and tested by me according to my registration number and the assignment requirements.

My personalised values are:

- Registration Number: IT23695702
- Last four digits: 5702
- Port Number: 11702
- NID: 6957
- Source files: server_5702.c, client_5702.c and Makefile_5702
- Storage directory: storage/IT23695702/

## Prompt 1 – Understanding TCP Socket Programming

Asked AI to explain how a TCP client and server communicate using socket(), bind(), listen(), accept(), connect(), send() and recv() in C.

**How I used it:**  
I used the explanation to understand the basic client-server communication before working on my implementation.

## Prompt 2 – Understanding Multi-Threading

Asked AI to explain why pthreads are needed when multiple clients connect to the same server.

**How I used it:**  
I used this explanation to understand how each connected client can be handled separately.

## Prompt 3 – Understanding TCP Message Handling

Asked AI to explain why TCP data may arrive in parts and why the program should not assume that one recv() call always contains one complete message.

**How I used it:**  
This helped me understand the reason for handling line-based commands and exact byte counts carefully.

## Prompt 4 – Understanding File Transfer

Asked AI to explain how a file can be transferred through a TCP socket and why the exact file size must be received.

**How I used it:**  
I used the explanation to understand and test the SENDFILE functionality in my implementation.

## Prompt 5 – Debugging

Asked AI to explain some compilation and runtime errors that occurred while testing the server and client.

Examples included understanding the "Address already in use" problem and checking socket-related errors.

**How I used it:**  
I checked the suggested explanations, corrected my code where necessary, compiled it again and tested the result in Ubuntu.

## Prompt 6 – Testing

Asked AI for suggestions on how to test the implementation using multiple terminals and multiple clients.

**How I used it:**  
I manually tested REGISTER, LIST, BCAST, PMSG, JOIN, LEAVE, ROOMS, RMSG, SENDFILE and presence notifications using different client connections.

## Personalisation and My Work

I customised the implementation according to my registration number IT23695702. I calculated and used my required port number and NID, named the source files using my registration digits, configured the personalised storage directory, compiled the programs and tested the functions in Ubuntu.

I also used Git during development to record the progress of the implementation.


