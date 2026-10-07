IE3010 NetMessenger
Registration Number: IT23695702
Port Number: 11702
NID: 6957

Files:
- server_5702.c - Multi-threaded NetMessenger server
- client_5702.c - NetMessenger client
- Makefile_5702 - Build instructions

Implemented Features:
- User registration
- Duplicate username checking
- Multiple concurrent clients using pthreads
- LIST connected users
- Broadcast messaging (BCAST)
- Private messaging (PMSG)
- Room creation and joining (JOIN)
- Leaving rooms (LEAVE)
- Listing rooms (ROOMS)
- Room messaging (RMSG)
- File transfer (SENDFILE)
- Persistent file storage
- User presence notifications
- Server logging with timestamps

Compile:
make -f Makefile_5702

Run Server:
./server_5702

Run Client:
./client_5702

Storage Directory:
storage/IT23695702/

Log File:
netmsg_IT23695702.log
