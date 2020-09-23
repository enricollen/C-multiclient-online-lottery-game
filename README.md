# C-multiclient-online-lottery-game
A simple multi-client shell application written in C which simulate the Lottery game

#DISCLAIMER
This application has been developed on Debian GNU/Linux 8.0, so that it won't run on Windows OS.

#INSTRUCTIONS FOR OFFLINE TEST (LOCALHOST)
1. Download the .zip file and extract it wherever you want (i.e. /home/Desktop)
2. Open 2 terminal and type cd Desktop/progetto
3. Now you have to compile both files "lotto_server.c" and "lotto_client.c", to do so type in the command "make" in terminal 
(If command "make" doesnt work for you, compile each files manually as follows:
  >> gcc -c lotto_server.c -o lotto_server.o
  >> gcc- lotto_server.o -o lotto_server
  >> gcc -c lotto_client.c -o lotto_client.o
  >> gcc- lotto_client.o -o lotto_client
)
4. Now run the server in first terminal typing "./lotto_server <port>"
  >> ./lotto_server 4444 Y (Y=minutes between lottery extraction)
5. Run client in second terminal typing "./lotto_client <ip addr> <port>"
  >> ./lotto_client 127.0.0.1 4444
6. Now you can start playing simply by following the list of available commands below.
  
#INSTRUCTION FOR ONLINE TEST
0. Firstly, you have to access your router administration panel by typing 192.168.1.1 into the URL search bar on your web browser.
1. Now you have to find the LOCAL Ip Address of the host you want to run the server onto. (i.e. 192.168.x.x or similar)
2. Now look for "Port Forwarding" or similar panel, and choose open a port X (i.e. 4444) for the host which has the Local Ip from the step 1.
3. Find the IP Address of the host (i.e. on Linux you can type "ip addr show" and check the "inet" field) and write it down somewhere.
4. You can now download the .zip file on 2 different host (the one which have the X port opened will run the server obviously)
5. Compile as in step 3 from previous instruction list
6. Run the server on the host which has the X port opened, and the client from another linux host from the rest of the world.
  >>./lotto_server X Y (X=forwarded port / Y=minutes between lottery extraction)
  >>./lotto_client IP_ADDR X  (IP_ADDR = the online ip from step 3)
5. Now you can start playing simply by following the list of available commands below.

#COMMAND LIST
Every command starts with "!", so "!<command>".
Once you are connected, you can type the command "!help" for the list of the whole available ones.
Then you can check the single command by typing "!help <command name>"
All the captions are in italian btw, so here's a quick translation for you:
1. !signup <username> <password> => signup the game
2. !login <username> <password> => login into the game with your credentials, requires signup
3. !invia_giocata -r <name of the city> -n <numbers you wanna bet, up to 10> -i <amount of $ for every bet, up to 5>
  (i.e. >>!invia_giocata -r roma napoli milano firenze torino venezia -n 23 5 36 89 3 57 -i 12 0 3 4 0)
  (it means you are betting 12$ on guessing a single number like 23, 5, 36... and 3$ on guessing that (23,89,36) or (57,5,3) or whatever combination of three numbers will be extracted in at least one of the city you played etc...)
4. !vedi_giocate 1 => check you bets, which are still waiting for the next extraction
5. !vedi_giocate 0 => check your expired bets. Extractions are calculated by the system every Y minutes.
6. !vedi_vincite => check your potential winnings, requires a previous !invia_giocata command.
7. !vedi_estrazione X => it shows the last X extractions calculated by the system, so that you can check the winning numbers on every city.
8. !vedi_estrazione X Y => it shows the last X extractions for the Y city, so that you can check the winning numbers on Y.
9. !esci => stands for quit, requires to be logged in
