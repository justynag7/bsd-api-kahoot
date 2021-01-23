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
#include <condition_variable>

class Question{
public:
    std::string questionText;
    std::string answearA, answearB, answearC, answearD;
    std::string correctAnswear;
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
    void addToScore(int amount)         {score += amount;}

    std::string getNickname()           {return nickname;}
    int getScore()                      {return score;}
    int getPlayerID()                   {return playerID;}

    void setNickname(std::string nick)  {nickname = nick;}
    void setScore(int scr)              {score = scr;}
    void setPlayerID(int id)            {playerID = id;}
};


// store player info
Player players[100];

int playersConnected = 0;

// server socket
int servFd;

std::condition_variable onlinePlayersCv;

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

// sends given questions to the players 
void askQuestion(Question q);


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

    
    // test question
    Question testQuestion = Question();
    testQuestion.questionText = "A is the correct answear.";
    testQuestion.answearA = "Answear 1";
    testQuestion.answearB = "Answear 2";
    testQuestion.answearC = "Answear 3";
    testQuestion.answearD = "Answear 4";
    testQuestion.correctAnswear = testQuestion.answearA;;
    

/****************************/

    
    std::thread([testQuestion]{
    std::mutex m;
    std::unique_lock<std::mutex> ul(m);
    printf("Question asking thread started.\n");
    onlinePlayersCv.wait(ul, [] { return (playersConnected >= 2) ? true : false; });
    printf("Two players logged in, asking the question\n");
        askQuestion(testQuestion);
    }).detach();
    
    
    
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
        //playersConnected ++;

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
    
    // TEST
    // Change condition variable so the question asking thread shuts down. 
    playersConnected = 2;
    onlinePlayersCv.notify_one();
    
    for(int clientFd : clientFds){
        const char* msg = "Server down!\n";
        if(send(clientFd, msg, strlen(msg)+1, MSG_DONTWAIT) != (int)strlen(msg) + 1){
            perror("Server down message error");
        }
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

        memset(buffer,0,255);
        if(read(clientFd,buffer,255) > 0){
            // Swapping '\n' for a null character
            buffer[strlen(buffer)-1] = '\0';
            printf("Player %s answeared %s\n",players[clientFd].getNickname().c_str(),buffer);
        }

        // stop the client from disconecting immediately (test)
        read(clientFd,buffer,255);
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
    const char* msg =  "Choose your nickname:\n";
    if(send(clientFd, msg, strlen(msg)+1, MSG_DONTWAIT) != (int)strlen(msg) + 1){
        std::unique_lock<std::mutex> lock(clientFdsLock);
        perror("Nickname setup message failed");
        clientFds.erase(clientFd);
    }
    char buffer[64];
    decltype(clientFds) bad;
    while(true)
    {
        memset(buffer,0,sizeof(buffer));
        if (recv(clientFd,buffer,64,MSG_FASTOPEN) > 0){
            // Swapping '\n' for a null character

            buffer[strlen(buffer)-1] = '\0';
            int r = strlen(buffer);
            if(validNickname(buffer) && r <= 16 && r >= 3){
                players[clientFd].setNickname(buffer);
                const char* msg =  "Nickname set !\n";
                if(send(clientFd, msg, strlen(msg)+1, MSG_DONTWAIT) != (int)strlen(msg) + 1){
                    std::unique_lock<std::mutex> lock(clientFdsLock);
                    perror("Nickname setup message failed");
                    clientFds.erase(clientFd);
                }
                playersConnected ++;
                onlinePlayersCv.notify_one();
                break;
            }
            else if(r < 3){
                const char* msg = "Nickname too short ! Try something with at least 3 characters:\n";
                if(send(clientFd, msg, strlen(msg)+1, MSG_DONTWAIT) != (int)strlen(msg) + 1){
                    std::unique_lock<std::mutex> lock(clientFdsLock);
                    perror("Nickname setup message failed");
                    clientFds.erase(clientFd);
                }
            }
            else if(r > 16){
                const char* msg = "Nickname too long ! Try something below 16 characters:\n";
                if(send(clientFd, msg, strlen(msg)+1, MSG_DONTWAIT) != (int)strlen(msg)+1){
                    perror("Nickname setup message failed");
                    std::unique_lock<std::mutex> lock(clientFdsLock);
                    clientFds.erase(clientFd);
                }
            }
            else
            {
                const char* msg = "Nickname already taken ! Try something different:\n";
                if(send(clientFd, msg, strlen(msg)+1, MSG_DONTWAIT) != (int)strlen(msg)+1){
                    perror("Nickname setup message failed");
                    std::unique_lock<std::mutex> lock(clientFdsLock);
                    clientFds.erase(clientFd);
                }
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

void askQuestion(Question q){
    int res;
    std::unique_lock<std::mutex> lock(clientFdsLock);
    decltype(clientFds) bad;
    for(int clientFd : clientFds){
        char msg[2048] = "\0";
        strcat(msg,q.questionText.c_str());
        strcat(msg,"\nA: ");
        strcat(msg,q.answearA.c_str());
        strcat(msg,"\nB: ");
        strcat(msg,q.answearB.c_str());
        strcat(msg,"\nC: ");
        strcat(msg,q.answearC.c_str());
        strcat(msg,"\nD: ");
        strcat(msg,q.answearD.c_str());
        strcat(msg,"\n");
        int count = strlen(msg);
        printf("Size of question : %d\n", count);
        res = send(clientFd, msg, count, MSG_DONTWAIT);
        if(res!=count)
            bad.insert(clientFd);
    }
    for(int clientFd : bad){
        printf("removing %d\n", clientFd);
        clientFds.erase(clientFd);
        close(clientFd);
    }

}