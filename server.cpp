#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <error.h>
#include <netdb.h>
#include <string.h>
#include <thread>
#include <mutex>
#include <unordered_set>
#include <signal.h>
#include <vector>
#include <set>

class Question{
private:
    std::string questionText;
    std::string answearA, answearB, answearC, answearD;
    std::string correctAnswear;

public:
    Question(){
    }

    // getters
    std::string getQuestion()       {return questionText;}
    std::string getA()              {return answearA;}
    std::string getB()              {return answearB;}
    std::string getC()              {return answearC;}
    std::string getD()              {return answearD;}
    std::string getCorrectAnswear() {return correctAnswear;}
    
    // setters
    void setQuestion(std::string question)       {questionText = question;}  
    void setA(std::string A)                     {answearA = A;}
    void setB(std::string B)                     {answearB = B;}
    void setC(std::string C)                     {answearC = C;}
    void setD(std::string D)                     {answearD = D;}
    void setCorrectAnswear(std::string correct)  {correctAnswear = correct;}

};

class Player{
private:
    std::string nickname;
    int playerID;
    int score;

public:
    Player(int id){
        playerID = id;
        nickname = "player ";
        nickname += std::to_string(id);
    }
    Player(){
        nickname = "offline";
    }
    void AddToScore(int amount)         {score += amount;}

    // setters
    std::string getNickname()           {return nickname;}
    int getScore()                      {return score;}
    int getPlayerID()                   {return playerID;}

    // getters
    void setNickname(std::string nick)  {nickname = nick;}
    void setScore(int scr)              {score = scr;}
    void setPlayerID(int id)            {playerID = id;}
};


// store player info
Player players[100];

int playersConnected = 0;

// server socket
int servFd;

// client sockets
std::mutex clientFdsLock;
std::unordered_set<int> clientFds;

// handles SIGINT
void ctrl_c(int);

// sends data to clientFds excluding fd
void sendToAllBut(int fd, char * buffer, int count);


// handles interaction with the client
void clientLoop(int clientFd, char * buffer);

// prompts client to provide a valid nickname
void setPlayerNickname(int fd);

// checks nickname availability
bool validNickname(std::string nickname);

// displays a nickname list of onilne players
void displayPlayers();


// converts cstring to port
uint16_t readPort(char * txt);

// sets SO_REUSEADDR
void setReuseAddr(int sock);

int main(int argc, char ** argv){
    // get and validate port number
    if(argc != 2) error(1, 0, "Need 1 arg (port)");
    auto port = readPort(argv[1]);
    
    // create socket
    servFd = socket(AF_INET, SOCK_STREAM, 0);
    if(servFd == -1) error(1, errno, "socket failed");
    
    // graceful ctrl+c exit
    signal(SIGINT, ctrl_c);

    // prevent dead sockets from raising pipe signals on write
    signal(SIGPIPE, SIG_IGN);
    
    setReuseAddr(servFd);
    
    // bind to any address and port provided in arguments
    sockaddr_in serverAddr{.sin_family=AF_INET, .sin_port=htons((short)port), .sin_addr={INADDR_ANY}};
    int res = bind(servFd, (sockaddr*) &serverAddr, sizeof(serverAddr));
    if(res) error(1, errno, "bind failed");
    
    printf("Server started.\n");

    printf("Listening ...\n");

    // enter listening mode
    res = listen(servFd, 1);
    if(res) error(1, errno, "listen failed");

    
    
/****************************/
    
    while(true){
        // prepare placeholders for client address
        sockaddr_in clientAddr{};
        socklen_t clientAddrSize = sizeof(clientAddr);
        
        // accept new connection
        auto clientFd = accept(servFd, (sockaddr*) &clientAddr, &clientAddrSize);
        if(clientFd == -1) error(1, errno, "accept failed");
        
        // add client to all clients set
        {
            std::unique_lock<std::mutex> lock(clientFdsLock);
            clientFds.insert(clientFd);
        }
        
        // tell who has connected
        printf("new connection from: %s:%hu (fd: %d)\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), clientFd);
        
        // create a new player
        Player p(clientFd);
        players[clientFd] = Player(clientFd);
        playersConnected ++;

        
        displayPlayers();

// client threads 
/******************************/
        std::thread([clientFd]{
            char buffer[255];
            clientLoop(clientFd, buffer);
        
        }).detach();   
    }
/*****************************/
}

uint16_t readPort(char * txt){
    char * ptr;
    auto port = strtol(txt, &ptr, 10);
    if(*ptr!=0 || port<1 || (port>((1<<16)-1))) error(1,0,"illegal argument %s", txt);
    return port;
}

void setReuseAddr(int sock){
    const int one = 1;
    int res = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    if(res) error(1,errno, "setsockopt failed");
}

void ctrl_c(int){
    std::unique_lock<std::mutex> lock(clientFdsLock);
    for(int clientFd : clientFds){
        shutdown(clientFd, SHUT_RDWR);
        close(clientFd);
    }
    close(servFd);
    printf("Closing server\n");
    exit(0);
}


void clientLoop(int clientFd, char * buffer){
    
    printf("%s has connected to the server\n", players[clientFd].getNickname().c_str());
    setPlayerNickname(clientFd);
    
    while(true){
        
        
        printf("removing %d\n", clientFd);
        {
                std::unique_lock<std::mutex> lock(clientFdsLock);
                clientFds.erase(clientFd);
                playersConnected --;
            }
        shutdown(clientFd, SHUT_RDWR);
        close(clientFd);
        break;
        
    }
    printf("Ending service for client %d\n", clientFd);
}

void setPlayerNickname(int clientFd){
    send(clientFd, "Choose your nickname:\n", 23, MSG_DONTWAIT);
    char buffer[64];

    while(true)
    {
        memset(buffer,0,sizeof(buffer));
        if (recv(clientFd,buffer,64,MSG_FASTOPEN) > 0){
            buffer[strlen(buffer)-1] = '\0';
            int r = strlen(buffer);
            if(validNickname(buffer) && r <= 16 && r >= 3){
                players[clientFd].setNickname(buffer);
                send(clientFd, "Nickname set !\n", 16, MSG_DONTWAIT);
                break;
            }
            else if(r < 3){
                send(clientFd, "Nickname too short ! Try something with at least 3 characters:\n", 64, MSG_DONTWAIT);
            }
            else if(r > 16){
                send(clientFd, "Nickname too long ! Try something below 16 characters:\n", 56, MSG_DONTWAIT);
            }
            else
            {
                send(clientFd, "Nickname already taken ! Try something different:\n", 51, MSG_DONTWAIT);
            }
        }

        else{
            printf("Client %d has disconnected !\n", clientFd);
            std::unique_lock<std::mutex> lock(clientFdsLock);
            clientFds.erase(clientFd);
            break;
        }
    }
}

bool validNickname(std::string nickname){
    for(int i : clientFds){
        // return false if name is already taken
        if (nickname == players[i].getNickname().c_str()){
            return false;
        }
    }
    return true;
}

void displayPlayers(){
    printf(" === Players online: %d===\n",playersConnected);
    for( int i : clientFds){
        printf("%s\n",players[i].getNickname().c_str());
    }
}