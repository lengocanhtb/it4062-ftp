#include <arpa/inet.h>  // htons(), inet_addr()
#include <errno.h>      // errno
#include <netinet/in.h> // inet_addr()
#include <stdbool.h>    // bool
#include <stdio.h>
#include <stdlib.h> // strtol()
#include <string.h> // bzero(), strlen(), strcmp(), strcpy(), strtok(), memcmp()
#include <sys/socket.h> // socket(), inet_addr(), connect(), recv(), send()
#include <sys/types.h>  // socket()
#include <unistd.h>     // close()
#include <sys/stat.h>

#define MAX 1024

int is_regular_file(const char *path)
{
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}
int begin_with(const char *str, const char *pre) {
  size_t lenpre = strlen(pre), lenstr = strlen(str);
  return lenstr < lenpre ? 0 : memcmp(pre, str, lenpre) == 0;
}

void client_ls(int sock, char *buffer) {
  // Send & Evaluate
  if (send(sock, buffer, strlen(buffer) + 1, 0) == -1) {
    fprintf(stderr, "can't send packet");
    perror("");
    return;
  }

  // Recieve
  char response[1024];
  if (recv(sock, response, sizeof(response), 0) == -1) {
    fprintf(stderr, "can't receive packet");
    perror("");
    return;
  }

  // Print
  if (begin_with(response, "@")) {
    printf("Server Error: %s\n", &response[1]);
  } else {
    printf("%s", response);
  }
}

void client_cd(int sock, char *buffer, char **path) {
  // Send & Evaluate
  if (send(sock, buffer, strlen(buffer) + 1, 0) == -1) {
    fprintf(stderr, "can't send packet");
    perror("");
    return;
  }

  // Recieve
  char response[1024];
  if (recv(sock, response, sizeof(response), 0) == -1) {
    fprintf(stderr, "can't receive packet");
    perror("");
    return;
  }

  // Print
  if (begin_with(response, "@")) {
    printf("%s\n", &response[1]);
  } else {
    free(*path);
    *path = malloc(strlen(response) + 1);
    strcpy(*path, response);
  }
}

void client_download(int sock, char *buffer, char *target_file) {
  if( access(target_file, F_OK ) == 0 ) {
    // file exists
    fprintf(stderr, "File already exists\n");
    return;
  } 
  ssize_t chunk_size;
  long received_size = 0;

  // Send Download Command
  if (send(sock, buffer, strlen(buffer) + 1, 0) == -1) {
    fprintf(stderr, "can't send packet");
    perror("");
    // fclose(fd);
    return;
  }

  // Retrieve File Size
  char response[1024];
  if (recv(sock, response, sizeof(response), 0) == -1) {
    fprintf(stderr, "can't receive packet");
    perror("");
    // fclose(fd);
    return;
  }
  if (begin_with(response, "@")) {
    printf("Server Error: %s\n", response);
    // fclose(fd);
    return;
  }
  // Initialize File Descriptor
  FILE *fd = fopen(target_file, "wb");
  if (fd == NULL) {
    fprintf(stderr, "can't create file");
    perror("");
    return;
  }
  long file_size = strtol(response, NULL, 0);

  // Notify server to start transmission
  strcpy(buffer, "ready");
  if (send(sock, buffer, strlen(buffer) + 1, 0) == -1) {
    fprintf(stderr, "can't send packet");
    perror("");
    fclose(fd);
    return;
  }

  // Start Receiving
  while (received_size < file_size &&
         (chunk_size = recv(sock, response, sizeof(response), 0)) > 0) {
    if (received_size + chunk_size > file_size) {
      fwrite(response, 1, file_size - received_size, fd);
      received_size += file_size - received_size;
    } else {
      fwrite(response, 1, chunk_size, fd);
      received_size += chunk_size;
    }
  }

  // Clean Up
  printf("%s Downloaded\n", target_file);
  fclose(fd);
}

void client_upload(int sock, char *buffer, char *target_file) {
    if (is_regular_file(target_file) == 0)
    {
        fprintf(stderr, "%s is a folder or not existed\n",target_file);
        // fclose(fd);
        return;
    }
    // Initialize File Descriptor
  FILE *fd = fopen(target_file, "rb");
  if (fd == NULL) {
    fprintf(stderr, "Error ");
    perror("Error");
    return;
  }
  // Send Upload Command
  if (send(sock, buffer, strlen(buffer) + 1, 0) == -1) {
    fprintf(stderr, "can't send packet");
    perror("");
    return;
  }
  
  ssize_t chunk_size;

  // Wait for server to be ready
  char response[1024];
  if (recv(sock, response, sizeof(response), 0) == -1) {
    fprintf(stderr, "can't receive packet");
    perror("");
    fclose(fd);
    return;
  }
  if (begin_with(response, "@")) {
    printf("Server Error: %s\n", &response[1]);
    fclose(fd);
    return;
  }

  // Notify File Size
  fseek(fd, 0L, SEEK_END);
  sprintf(buffer, "%ld", ftell(fd));
  if (send(sock, buffer, strlen(buffer) + 1, 0) == -1) {
    fprintf(stderr, "can't send packet");
    perror("");
    fclose(fd);
    return;
  }
  fseek(fd, 0L, SEEK_SET);

  // Wait for server to be ready
  if (recv(sock, response, sizeof(response), 0) == -1) {
    fprintf(stderr, "can't receive packet");
    perror("");
    fclose(fd);
    return;
  }

  // Start Transmission
  while ((chunk_size = fread(buffer, 1, sizeof(buffer), fd)) > 0) {
    if (send(sock, buffer, chunk_size, 0) == -1) {
      fprintf(stderr, "can't send packet");
      perror("");
      fclose(fd);
      return;
    }
  }

  // Clean Up
  printf("%s Uploaded\n", target_file);
  fclose(fd);
}

void client_mkdir(int sock, char *buffer, char *new_dir) {
  // Send & Evaluate
  if (send(sock, buffer, strlen(buffer) + 1, 0) == -1) {
    fprintf(stderr, "can't send packet");
    perror("");
    return;
  }

  // Recieve
  char response[1024];
  if (recv(sock, response, sizeof(response), 0) == -1) {
    fprintf(stderr, "can't receive packet");
    perror("");
    return;
  }

  // Print
  if (begin_with(response, "@")) {
    printf("%s\n", &response[1]);
  } 
}

void client_rm(int sock, char *buffer, char *target_file){
  if (send(sock, buffer, strlen(buffer) + 1, 0) == -1) {
    fprintf(stderr, "can't send packet");
    perror("");
    return;
  }

  char response[1024];
  if (recv(sock, response, sizeof(response), 0) == -1) {
    fprintf(stderr, "can't receive packet");
    perror("");
    return;
  }

  if (begin_with(response, "@")) {
    printf("%s\n", &response[1]);
  } 
}

void client_move(int sock, char *buffer, char *target_file) {
  // send command
  if (send(sock, buffer, strlen(buffer) + 1, 0) == -1) {
    fprintf(stderr, "can't send packet");
    perror("");
    return;
  }

  // receive response
  char response[1024];
  if (recv(sock, response, sizeof(response), 0) == -1) {
    fprintf(stderr, "can't receive packet");
    perror("");
    return;
  }
  if (begin_with(response, "@")) {
    printf("%s\n", &response[1]);
    return;
  } 

  char target_path[MAX];
  // send target folder
  printf("Target directory path: ");
  fgets(target_path,MAX,stdin);
  target_path[strcspn(target_path,"\n")] = '\0';
  if (send(sock, target_path, strlen(target_path) + 1, 0) == -1) {
    fprintf(stderr, "can't send packet");
    perror("");
    return;
  }

  // receive response
  memset(response, '\0', MAX);
  if (recv(sock, response, sizeof(response), 0) == -1) {
    fprintf(stderr, "can't receive packet");
    perror("");
    return;
  }
  if (begin_with(response, "@")) {
    printf("%s\n", &response[1]);
    return;
  }
  printf("%s: Moved\n",target_file);
  return;
} 

void client_process(int sock, char *buffer, char **path) {
  // Prepare
  char *full_command = malloc(strlen(buffer) + 1);   
  strcpy(full_command, buffer);
  char *delim = " ";
  char *command = strtok(full_command, delim);
  char *context = strtok(NULL, delim);

  // Process
  if (begin_with(command, "ls")) {
    client_ls(sock, buffer);
  } else if (begin_with(command, "cd")) {
    client_cd(sock, buffer, path);
  } else if (begin_with(command, "download")) {
    client_download(sock, buffer, context);
  } else if (begin_with(command, "upload")) {
    client_upload(sock, buffer, context);
  } else if (begin_with(command,"mkdir")) {
    client_mkdir(sock,buffer,context);
  } else if (begin_with(command,"rm")) {
    client_rm(sock,buffer,context);
  } else if (begin_with(command,"move")) {
    client_move(sock,buffer,context);
  } else {
    printf("No such command: %s\n", buffer);
  }

  // Cleanup
  free(full_command);
}

int main(int argc, const char *argv[]) {
  // check argument
  // check argument
 	if(argc != 3){
 		printf("Invalid argument\n\n");
 		return 0;
 	}

  // Create Socket
  int sock; //clientfd for client;
  if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    perror("Can't create socket");
    exit(errno);
  }

  // Initialize Address, Buffer, Path
  struct sockaddr_in server_addr;
  char buffer[1024];
  char buff[MAX];
  bzero(&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(atoi(argv[2]));
  server_addr.sin_addr.s_addr = inet_addr(argv[1]);
  char username[MAX], password[MAX];
  int bytes_sent, bytes_received;
  char *path = malloc(2);
  strcpy(path, ".");
  int LOGIN = 0, PATH = 0;
  // Connecting to Server
  if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
    perror("Connect Failed");
    close(sock);
    exit(errno);
  }
  
  while (LOGIN == 0){
    puts("Please enter username and password: ");
    printf("Username: ");
    fgets(username,MAX,stdin);
    username[strcspn(username,"\n")] = '\0';

    // send username to serv
    if (0 >= (bytes_sent=send(sock,username,strlen(username),0)))
    {
      printf("\n Connection closed\n");
      return 0;
    }
    // receive server reply
    if (0 >= (bytes_received = recv(sock,buff,MAX-1,0)))
    {
      printf("\nError!Cannot receive data from sever!\n");
      return 0;
    }
    buff[bytes_received] = '\0';
    if (0 == strcmp(buff,"0"))
    {
      puts("\nAccount not existed.Please try again\n");
      continue;
    }
    
    /* get pass */
    printf("Password: ");
    fgets(password,MAX,stdin);
    password[strcspn(password, "\n")] = '\0';

    // send pass to server
    if (0 >= (bytes_sent = send(sock, password,strlen(password),0)))
    {
      printf("\nConnection closed!\n");
      return 0;
    }

    memset(buff, '\0', MAX);
    if (0 >= (bytes_received = recv(sock,buff,MAX-1,0)))
    {
      printf("\nError!Cannot receive data from sever!\n");
      return 0;
    }
    buff[bytes_received] = '\0';
    if (0 == strcmp(buff,"0"))
    {
      puts("\nPassword is not correct. Please try again\n");
      continue;
    }    
    else if (0 == strcmp(buff,"1"))
    {
      puts("\nlogin is successfull\n"); 
    } 
    LOGIN = 1;    
  }

  if (0 >= (bytes_received = recv(sock,path,MAX-1,0)))
  {
    printf("\nFailed to receive folder!\n");
    return 0;
  }
  path[bytes_received] = '\0';
    
  // Read-Evaluate-Print Loop
  while (true) {
    // Read
    printf("(%s) $ ", path);
    fgets(buffer, 1024, stdin);
    if (strcmp(buffer, "\n") == 0) {
      continue; // Empty Input, reset
    }
    if ((strlen(buffer) > 0) && (buffer[strlen(buffer) - 1] == '\n')) {
      buffer[strlen(buffer) - 1] = '\0'; // Standard Input, fix EOL
    }
    if (strcmp(buffer, "exit") == 0) {
      break; // Exit, break loop
    }

    // Processing
    client_process(sock, buffer, &path);
  }
  printf("Session Terminated\n");
  close(sock);
  free(path);
}