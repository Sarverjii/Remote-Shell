#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <poll.h>

int sockFD, newsockFD;
int shell_in, shell_out; 




int isShellOutAvailable() {
    struct pollfd pfd;
    pfd.fd = shell_out;
    pfd.events = POLLIN;  // Check for data to read

    int ret = poll(&pfd, 1, 100); // Timeout of 100ms
    return (ret > 0) && (pfd.revents & POLLIN);
}



void sendPrompt()
{
    write(shell_in, "pwd\n", 4);
    char buffer[256];
    memset(buffer, 0 , sizeof(buffer));
    int bytes_read = read(shell_out,buffer,sizeof(buffer));

    if (buffer[bytes_read-1] == '\n') {
        buffer[bytes_read-1] = '\0';
        bytes_read--;
    }


    char prompt[300];
    snprintf(prompt, sizeof(prompt), "\n\033[92m%s>\033[0m ", buffer);

    send(newsockFD, prompt, strlen(prompt), 0);

}

void socketCreationAndBind(int portnum) {
    struct sockaddr_in server_addr;
    sockFD = socket(AF_INET, SOCK_STREAM, 0);
    if (sockFD < 0) {
        perror("ERROR CREATING SOCKET");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(portnum);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockFD, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("ERROR BINDING SOCKET");
        exit(1);
    }
}

void ReadandWrite(){
    char output[1024];
    char response[4096];
    while(1){
        sendPrompt();

        //Reading
        memset(output,0,sizeof(output));
        int bytes_received = recv(newsockFD, output, sizeof(output)  - 1, 0);
        if (bytes_received <= 0 || (strncmp(output, "exit", 4) == 0)) {
            perror("Client Disconnected");
            close(newsockFD);
            close(sockFD);
            exit(1);
        }
        output[bytes_received] = '\0';

        write(shell_in, output, bytes_received);
        //Now Writing

        if (isShellOutAvailable()) {
            int bytes_read = read(shell_out, response, sizeof(response));
            if (bytes_read > 0) {
                send(newsockFD, "\033[93mReply from Server : \033[94m\n", sizeof("\033[93mReply from Server : \033[94m\n"), 0);
                send(newsockFD, response, bytes_read, 0);
            }
        }
    }
}

void ShellProcess(){
    int readfd[2], writefd[2];

    if(pipe(readfd) == -1 || pipe(writefd) == -1)
    {
        perror("Error in setting up the pipe");
        exit(1);
    }
    int pid = fork();
    if(pid == -1)
    {
        perror("ERROR OPENING FORK");
        exit(1);
    }
    if(pid==0)
    {
        close(readfd[1]);
        close(writefd[0]);

        dup2(readfd[0], STDIN_FILENO);
        dup2(writefd[1], STDOUT_FILENO);
        dup2(writefd[1], STDERR_FILENO);

        close(readfd[0]);
        close(writefd[1]);
        execl("/bin/bash","/bin/bash",NULL);
        perror("Error opening Shell");
        exit(1);
    }
        close(readfd[0]);
        close(writefd[1]);

        shell_in = readfd[1];
        shell_out = writefd[0];

}


void AcceptingConnection() {
    struct sockaddr_in client_addr;
    socklen_t client_length = sizeof(client_addr);

    newsockFD = accept(sockFD, (struct sockaddr *)&client_addr, &client_length);
    if (newsockFD < 0) {
        perror("ERROR GETTING CLIENT SOCKET");
        exit(1);
    }


    ShellProcess();
    
    ReadandWrite();

    
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "\033[1;31mUsage: %s <Port>\033[0m\n", argv[0]);
        exit(1);
    }

    socketCreationAndBind(atoi(argv[1]));
    listen(sockFD, 5);
    AcceptingConnection();




    close(newsockFD);
    close(sockFD);
    return 0;
}
