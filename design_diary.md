# IE3010 NetMessenger – Design Diary

**Registration Number:** IT23695702  
**Port Number:** 11702  
**NID:** 6957

## 1. Starting My Implementation

First I started by understanding how a TCP server and client work in C. Then I created my server and client files and checked whether the client can connect with the server properly.

According to my registration number, I changed the required details for my implementation. I used port number 11702 and NID 6957.

At the beginning I tested the basic connection first before adding other functions.

## 2. User Registration

After the connection was working, I worked on the REGISTER function.

When a client connects, the user has to register with a username first. I also checked the duplicate username case. If the same username is already connected, the server should not allow another client to use that username.

I tested this using different terminals.

## 3. Multiple Clients

Next I worked on connecting more than one client at the same time.

For this I used pthreads. Each client is handled separately by the server. I opened different terminals and used usernames like Amal and Ravi to test it.

This helped me understand how one server can communicate with multiple clients at the same time.

## 4. Messaging

After that I tested the messaging functions.

First I tested LIST to check the currently connected users. Then I tested BCAST to send a message to the other connected clients.

I also tested PMSG for private messages between two users.

While testing this part, I learned that I have to check both sender and receiver terminals because some outputs are shown only on the receiving client's side.

## 5. Rooms

Next I worked on the room functions.

I tested JOIN to join a room and ROOMS to check the available rooms. I also tested LEAVE to leave a room.

After that I tested RMSG. I connected two users, joined both users to the same room and sent a room message. Then I checked whether the other user received the message correctly.

## 6. File Transfer

The file transfer part was a little difficult for me.

While doing this part, I understood that TCP sends data as a stream of bytes. So I cannot always expect the complete file to come from only one recv() call.

I tested SENDFILE between two clients. I also checked the received file and checked whether the server stored the file correctly inside:

`storage/IT23695702/`

Finally, the file transfer worked correctly between my two test users.

## 7. Problems I Faced

During this implementation I faced some errors.

One main problem was the "Address already in use" error. This happened when I stopped and started the server again during testing. I checked the running processes and also used SO_REUSEADDR in my server.

I also got some errors and warnings when I was working on the file transfer part. I checked the compiler messages and corrected them step by step.

Testing using separate terminals helped me a lot because I could see what was happening on the server, sender and receiver sides.

## 8. Presence and Logging

After the main functions were working, I added presence notifications.

When a new user connects, the other connected user can see that the user joined. When the user disconnects, the other user can also see that the user left.

I also added a log file with timestamps. I tested it by registering and disconnecting users and then checked the log file from the terminal.

## 9. Git

I used Git while doing my implementation.

I made commits after completing different parts of the project. For example, I made commits for registration, multiple clients, LIST, messaging, rooms, file transfer, presence notifications and logging.

This helped me keep the changes of my project step by step.

## 10. What I Learned

From this implementation I learned more about TCP socket programming and how a server and client communicate.

I also understood more about pthreads, multiple clients, sending messages, rooms and transferring files through TCP.

I learned that testing is very important. When I added a new function, I tested it before going to the next part. If I got an error, I checked the error and tried to understand where the problem was.

Finally, I tested the main functions with multiple clients and confirmed that my NetMessenger implementation was working.
