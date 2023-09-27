Author: Anthony Paulino 

Run service & client:
-----------------------------
Two clients
- ttt_1.c is user-friendly, rejects malformed messages (DEBUGGING FOR APPLICATION/GAME LEVEL ERRORS)
- ttt_2.c is not user-friendly, accepts malformed messages (DEBUGGING MAINLY FOR PRESENTATION LEVEL ERRORS)

./ttts 15100
./ttt_1 cd.cs.rutgers.edu 15100 (Recommended)
./ttt_2 cd.cs.rutgers.edu 15100 (For Debugging) 
-----------------------------
User input commands for ttt_1.c :
PLAY|"name"| - To begin game with specific username.
MOVE|ROW|COLUMN| - Make a move. (1<=ROW,COLUMN<=3)
DRAW| - Propose a draw.
DRAW|A| or DRAW|R| - Accept or reject a draw.
RSGN| - Resign.
------------------------------
Multithreading & use of locks 
-----------------------------
Main thread will be handling incoming connections in a loop, once a connection is accepted by the server it will create a thread to handle the connection and wait for its "PLAY" message. 
Therefore a thread per each accepted connections, this thread will be waiting for clients PLAY message. 
A global struct called pair_t, with variable name pair_status will keep track of when the server has two players ready to be thrown into a game. A player_t struct with variable name players will keep track of all players that are ready to play(player list), or in a game. Players that have finished the game will be removed from this list. player_t struct holds  the name of the player its handle_t varaible to access its socket & buffer and whether wherther their id in the list is avaible or in use(the struct uses this to maintain list).
Everytime the thread that handles accepted connection is going to access the pair_statusm and players variable it uses mutex to ensure that other threads are not accessing it at the same time.
The pair_status will have two slots and will be set to NULL to indicate a slot for a player is empty. If a slot is NULL, the thread will put a client thats ready to play in that slot, when the two slots are filled up the thread that fills in the last empty slot will detect that pair_status is ready, it will create a game thread for those two cleints and empty pair_status for the other threads to continue using the variable.
 
The process will go something like this:
Main thread will be listening to incoming connections through get_client(), it will return a socket file descriptor and the main thread will proceed to create a handle_t variable that will hold the connections file descriptor, a buffer and other int varaibles to manage the buffer when connection/game thread is reading from the clients socket. 
A buffer per client will ensure that threads will not be using the same buffer, making the read to a client thread saftey and synchoronized.
A thread will be created to serve as a "waiting room" for each client that wants to play the game, this "waiting room" is the pair_clients() function, the handle_t variable will passed on to this thread and run pair_clients(), the handle_t variable will have the socket file descriptor & buffer to call read and wait for the "PLAY" message from the client.
When the thread has recieved a valid "PLAY" message(unique username, name is not too long) a player will be created with create_player() with the use of mutex so that a thread will be inserting a player at a time. 
Then pair_clients() thread will check if pair_status slot 1 is NULL, if it is it will assign its player_t variable that was created in the thread to slot 1, if it is not it will check if slot 2 is empty and if it is it will assign its player_t variable to slot 2.
It will access pair_status with a mutex to ensure safety, essentially slot 2 will never be filled since we always fill up slot 1 first, then proceed to check if slot 2 is filled which will always be empty if slot 1 is filled so thread knows to create a game thread and empty slot 1 so that future pair_clients() threads handling connection can keep using pair_status to create games. 
When slot 2 is empty thread will not assign it to slot 2 since it will end up setting it NULL anyways so we just set slot 1 to NULL, and send a game_t variable that holds both players information and the state of their grid. Connection threads will almost exist, no matter what, if message is malformed it exit, if it creates a player and does not have another player to create a game it exit, if two players are ready to be inserted in a game a thread is created and then connection thread exits. 
Therefore, connection threads are always exiting regardless the circumstances unless read blocks indefintely waiting for client PLAY message. The game thread will run until the game is over and then exit. 
We do not have to worry about threads accessing pair_status at the same time/ accessing pair_status when it is full, since the use of mutex ensure that when a slot filled up and another one isn't it creates the player, makes a game_t variable and creates a game thread for those two players first. This process of creating a player and and assigning slots are done 1 at a time with the use of mutex therefore it is synchronized. 
-----------------------------
^C to terminate server, all games should be over, no accepted connections in the "Waiting Room".
read() will block indefinitely, if you accept a connection and client does not send their "PLAY" message for whatever reason the server will be waiting for it and it will block in a non-main thread so the server program will not terminate. As long as both these conditions are met, server can be disrupted with ^C.

Player Capacity: 300 connected players, can be adjusted.

One small bug that can easily be fixed, the server sends the game winning MOVD when it shouldn't and just send both clients the OVER message.
-----------------------------

Correct Inputs by user:
PLAY|X|
PLAY|O|
MOVE|1|1|
MOVE|1|2|
MOVE|1|3|
MOVE|2|1|
MOVE|2|2|
MOVE|2|3|
MOVE|3|1|
MOVE|3|2|
MOVE|3|3|
DRAW|
DRAW|A|
DRAW|R|
RSGN| 
------------------------------
Test Cases(Presentation Level):
Correct: PLAY|10|Joe Smith|  
Correct: PLAY|4|Ant|
Incorrect(4th Fields): PLAY|12|Joe Smith|X|
Incorrect(2nd Fields): PLAY|0|
Incorrect(Length): PLAY|9|Joe Smith|

Correct: MOVE|6|X|2,2| 
Incorrect(2nd field): MOVE|6|xxx|.|
Incorrect(3rd field): MOVE|6|F|2,2|
Incorrect(4th field): MOVE|6|X|0,2|
Incorrect(4th field): MOVE|6|X|1,0|
Correct: MOVE|6|O|1,2| 
Incorrect(3rd field): MOVE|8|X|1,1|X|
Incorrect(2nd field): MOVE|0|
Incorrect(3rd field & length): MOVE|4|X|2|
Incorrect(length): MOVE|5|X|2,2|
Incorrect(4th field): MOVE|8|X|2,2|X|

Correct: DRAW|2|S|
Correct: DRAW|2|A|
Correct: DRAW|2|R|
Incorrect(3rd field): DRAW|2|B|
Incorrect(4th/5th field): DRAW|7|S|X|ZX|
Incorrect(2nd field): DRAW|0|
Incorrect(2nd length): DRAW|1|S|
Incorrect(length): DRAW|1|A|

Correct: RSGN|0|
Incorrect(3rd field): RSGN|2|X|
Incorrect(3rd/4th field): RSGN|4|X|Y|

Correct: WAIT|0|
Incorrect(3rd field): WAIT|2|X|
WAIT|4|X|Y|
Incorrect(3rd/4th field & length):WAIT|3|X|Y|

Correct: BEGN|12|X|Joe Smith|
Incorrect(length): BEGN|55|X|Joe Smith|
Correct: BEGN|6|a|Foo|
Incorrect(2nd field): BEGN|0|
Incorrect(3rd field): BEGN|9|Joe Smith|
Incorrect(5th field): BEGN|14|O|Joe Smith|X|

Correct: MOVD|16|X|2,2|....X....|
Incorrect(3rd field): MOVD|16|Z|2,2|....X....|
Incorrect(4th field): MOVD|16|X|0,2|....X....|
Incorrect(length): MOVD|15|X|2,2|OXOXOXOXO|

Correct: INVL|24|That space is occupied.|
Incorrect: INVL|24|

All correct:
OVER|26|W|Joe Smith has resigned.|
OVER|26|L|Joe Smith has resigned.|
OVER|26|D|Joe Smith has resigned.|
Incorrect:
OVER|26|F|Joe Smith has resigned.|
OVER|23|F|Joe Smith has resigned.|

Special Cases:
Correct(should accept the first complete message and leave "BEGN" at the buffer):
- MOVE|6|X|2,2|BEGN

Correct PLAY message, second MOVE message incorrect should reject, since it only has its fields data invalid we don't close connection, we can keep reading and detect next MOVE message is correct
- PLAY|10|Joe Smith|MOVE|6|X|...|MOVE|6|X|2,2|


Correct(should accept the first complete message and leave "MOVE" at the buffer):
- BEGN|12|X|Joe Smith|MOVE
Can follow the message and write something to complete the message
- |6|X|2,2|

Very long message to see if it parses them correctly 
- PLAY|10|Joe Smith|MOVE|6|F|2,2|
- *It should accept the first PLAY message, reject the MOVE message(this isn't a malformed message, field is just invalid, therefore we dont close here we can keep reading).

- DRAW|2|A|RSGN|4|X|Y|
- *It should accept the first DRAW message, reject the RSGN message(since this is a malformed message, incorrect field counts, we'd stop here).

BEGN|12|X|Joe Smith|WAIT|0|MOVD|16|X|2,2|OXOXOXOXO|INVL|24|That space is occupied.|
- *It should accept the first BEGN message, the WAIT message, the MOVE message, and the INVL mesage.
-----------------------------
Test Cases(Application Level):
Pre-Game:
- Same user name, is not allowed
- malformed messages, closes the connetion for client in the waiting room 
- anything other than PLAY will not be expected, and will return invalid for unexpected message
In-Game:
- Unexecpted messages respond with invalid messages
    - any messages that are not MOVE,RSGN, and DRAW are rejected
- malformed messages, closes connections and ends the game 
-----------------------------
Test Cases(Game Level):
    - Invalid moves
        - the move has already been done
        - no draw S was suggested, and a player tries to do DRAW A or DRAW R
        - draw S was suggested, and other player tries to do DRAW S

All the four ways the game ends has been tested.
Resign, an agreed draw, board fills up, and winner/looser
