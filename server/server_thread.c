#include <arpa/inet.h>  // htons(), inet_addr()
#include <dirent.h>     // opendir(), readdir(), closedir()
#include <errno.h>      // errno
#include <netinet/in.h> // inet_addr(), bind()
#include <pthread.h>    // pthread_create(), pthread_exit()
#include <signal.h>     // signal()
#include <stdbool.h>    // bool
#include <stdio.h>
#include <stdlib.h> // strtol()
#include <string.h> // bzero(), strlen(), strcmp(), strcpy(), strtok(), strrchr(), memcmp()
#include <sys/socket.h> // socket(), inet_addr(), bind(), listen(), accept(), recv(), send()
#include <sys/types.h> // socket()
#include <unistd.h>    // close()

#define PortNumber 5555
#define MaxClient 20
#define MAX 100

int sock; //listenfd for server;

typedef struct node
{
  char username[MAX];
  char pass[MAX];
  int status;
  struct node *next;
} node_a;

// file -> linked list 
node_a *loadData(char *filename){
	int status, count =0;
	FILE *f;
	char username[MAX], pass[MAX];
	node_a *head, *current;
	head = current = NULL;

	printf("Loading data...\n");
	// open file
	if((f = fopen(filename,"r"))==NULL){
		printf("Failed to open file");
		exit(0);
	}

	//data -> linked list
	while(fscanf(f,"%s %s %d\n", username,pass,&status) != EOF){
		node_a *node = malloc(sizeof(node_a));
		strcpy(node->username, username);
		strcpy(node->pass,pass);
		node->status = status;

		if(head == NULL)
			current = head = node;
		else
			current = current->next = node;
		count++;
	}

	fclose(f);
	printf("Successfully loaded %d account(s)\n",count);
	return head;
}

//find account
node_a *findNode(node_a *head, char *username){
	node_a *current = head;
	while(current != NULL){
		if (0 == strcmp(current->username, username))
			return current;
		current = current->next;
	}

	return NULL;
}

// linked list -> file
void saveData(node_a *head, char *filename){
	FILE *f;
	f = fopen(filename,"w");
	node_a *current;
	for (current = head; current; current = current->next)
		fprintf(f, "%s %s %d\n", current->username, current->pass, current->status);
	fclose(f);
}


// xem command str có bắt đầu bằng xâu pre (VD begin_with(command,"ls") trả về 1 nếu command bắt đầu bằng ls)
int begin_with(const char *str, const char *pre) {
  size_t lenpre = strlen(pre), lenstr = strlen(str);
  return lenstr < lenpre ? 0 : memcmp(pre, str, lenpre) == 0;
}

// gửi thông điệp (phản hồi) lại client
int respond(int recfd, char response[]) {
  if ((send(recfd, response, strlen(response) + 1, 0)) == -1) {
    fprintf(stderr, "(%d) can't send packet\n", recfd);
    return errno;
  }
  return 0;
}

// xem cac tep trong thu muc (~lenh ls trong ubuntu) 
void server_ls(int recfd, char *response, char **current_path) {
  // Open Directory
  DIR *current_fd;
  if ((current_fd = opendir(*current_path)) == NULL)// opendir return pointer to directory stream and on error NUll
  {
    strcpy(response, "@Can't open ");
    strcat(response, *current_path);  //send to client 
    fprintf(stderr, "(%d) Can't open %s", recfd, *current_path); //remain in server
    perror("");
    return;
  }

  // Read Directory
  //d_type = d_type is the term used in Linux kernel that stands for “directory entry type”
  // struct dirent: Đây là một kiểu cấu trúc được sử dụng để trả về thông tin về các mục trong thư mục. Nó chứa các trường sau
  // https://www.gnu.org/software/libc/manual/html_node/Directory-Entries.html
  struct dirent *dir_entry;
  while ((dir_entry = readdir(current_fd)) != NULL) {
    // tra ve cac loai file/folder
    switch (dir_entry->d_type) {
    case DT_BLK:
      strcat(response, "block device\t\t");
      break;
    case DT_CHR:
      strcat(response, "character device\t\t");
      break;
    case DT_DIR:
      strcat(response, "directory\t\t");
      break;
    case DT_FIFO:
      strcat(response, "named pipe (FIFO)\t\t");
      break;
    case DT_LNK:
      strcat(response, "symbolic link\t\t");
      break;
    case DT_REG:
      strcat(response, "regular file\t\t");
      break;
    case DT_SOCK:
      strcat(response, "UNIX domain socket\t");
      break;
    default:
      strcat(response, "Unknown\t\t\t");
      break;
    }
    strcat(response, dir_entry->d_name);
    strcat(response, "\n");
  }

  // Close Directory
  if (closedir(current_fd) < 0) {
    fprintf(stderr, "(%d) Directory Close Error", recfd);
    perror("");
  }
}

// thay doi remote directory
void server_cd(int recfd, char *open_dir, char *response, char **current_path) {
  // Handle empty arg and . and ..
  // xử lý đối số trống . và .. 

  // neu khong co ten file (vd: cd ) => thong bao no directory given
  if (open_dir == NULL) {
    strcpy(response, "@no directory given");
    return;
    // neu "cd ."=> response = thu muc hien tai
  } else if (strcmp(open_dir, ".") == 0) {
    strcpy(response, *current_path);
    return;
    // "cd .."
  } else if (strcmp(open_dir, "..") == 0) {
    // Check Root
    // neu current_path = . => thong bao da den goc
    if (strcmp(*current_path, ".") == 0) {
      strcpy(response, "@already reached root");
      return;
    }
    // Truncate current path - cắt bớt đường dẫn hiện tại
    // failed
    char *trunct = strrchr(*current_path, '/');
    strcpy(trunct, "\0");
    strcpy(response, *current_path);
    return;
  }

  // Open Directory
  DIR *open_dir_fd;
  if ((open_dir_fd = opendir(*current_path)) == NULL) {
    strcpy(response, "@can't open");
    strcat(response, *current_path);
    fprintf(stderr, "(%d) Can't open %s", recfd, *current_path);
    perror("");
    return;
  }

  // Check existance
  bool exist = false;
  struct dirent *dir_entry = NULL;
  // tìm từ đầu đến cuối thư mục hiện tại (current_path)
  while ((dir_entry = readdir(open_dir_fd)) != NULL && !exist) {
    if (dir_entry->d_type == DT_DIR && strcmp(dir_entry->d_name, open_dir) == 0) {
      // Build new path name
      char *new_path = malloc(strlen(*current_path) + strlen(dir_entry->d_name) + 2);
      strcpy(new_path, *current_path);
      strcat(new_path, "/");
      strcat(new_path, dir_entry->d_name);

      // Store current path ~ đổi current_path
      free(*current_path);
      *current_path = malloc(strlen(new_path));
      strcpy(*current_path, new_path);
      strcpy(response, *current_path);
      free(new_path);
      exist = true;
    }
  }
  if (!exist) {
    strcpy(response, "@");
    strcat(response, *current_path);
    strcat(response, "/");
    strcat(response, open_dir);
    strcat(response, " does not exist");
  }

  // Close Directory
  if (closedir(open_dir_fd) < 0) {
    fprintf(stderr, "(%d) Directory Close Error", recfd);
    perror("");
  }
}

// download file
void server_download(int recfd, char *target_file, char **current_path) {

  // Build Path
  char *full_path = malloc(strlen(*current_path) + strlen(target_file) + 2);
  strcpy(full_path, *current_path);
  strcat(full_path, "/");
  strcat(full_path, target_file);

  // Initialize File Descriptor, Buffer
  FILE *fd;
  if ((fd = fopen(full_path, "rb")) == NULL) {
    respond(recfd, "@file open error");
    fprintf(stderr, "(%d) Can't open %s", recfd, full_path);
    perror("");
    return;
  }
  char buffer[1024];
  ssize_t chunk_size;

  // Notify File Size
  fseek(fd, 0L, SEEK_END);  //0L => 0 long means 0.000KB/Mb file, SEEK_END => end of file;
  sprintf(buffer, "%ld", ftell(fd)); // ftell(fd) current position
  ssize_t byte_sent = send(recfd, buffer, strlen(buffer) + 1, 0);
  if (byte_sent == -1) {
    fprintf(stderr, "(%d) can't send packet", recfd);
    perror("");
    fclose(fd);
    return;
  }
  fseek(fd, 0L, SEEK_SET); 

  // Wait for client to be ready
  ssize_t byte_received = recv(recfd, buffer, sizeof(buffer), 0);
  if (byte_received == -1) {
    fprintf(stderr, "(%d) can't receive packet", recfd);
    perror("");
    fclose(fd);
    return;
  }

  // Start Transmission
  while ((chunk_size = fread(buffer, 1, sizeof(buffer), fd)) > 0) {
    ssize_t byte_sent = send(recfd, buffer, chunk_size, 0);
    if (byte_sent == -1) {
      fprintf(stderr, "(%d) can't send packet", recfd);
      perror("");
      fclose(fd);
      return;
    }
  }
  printf("(%d) Transmited: %s\n", recfd, target_file);
  fclose(fd);
}

void server_upload(int recfd, char *target_file, char **current_path) {

  // Build Path
  char *full_path = malloc(strlen(*current_path) + strlen(target_file) + 2);
  strcpy(full_path, *current_path);
  strcat(full_path, "/");
  strcat(full_path, target_file);

  // Initialize File Descriptor
  FILE *fd;
  if ((fd = fopen(full_path, "wb")) == NULL) {
    respond(recfd, "@file open error");
    fprintf(stderr, "(%d) Can't open %s", recfd, *current_path);
    perror("");
    return;
  }

  // Retrieve File Size
  char buffer[1024];
  strcpy(buffer, "size?");
  ssize_t byte_sent = send(recfd, buffer, strlen(buffer) + 1, 0);
  if (byte_sent == -1) {
    fprintf(stderr, "(%d) can't send packet", recfd);
    perror("");
    fclose(fd);
    return;
  }
  ssize_t byte_received = recv(recfd, buffer, sizeof(buffer), 0);
  if (byte_received == -1) {
    fprintf(stderr, "(%d) can't receive packet", recfd);
    perror("");
    fclose(fd);
    return;
  }
  long file_size = strtol(buffer, NULL, 0);  //strol => coveert string to long int

  // Notify client to start transmission
  strcpy(buffer, "ready");
  byte_sent = send(recfd, buffer, strlen(buffer) + 1, 0);
  if (byte_sent == -1) {
    fprintf(stderr, "(%d) can't send packet", recfd);
    perror("");
    fclose(fd);
    return;
  }

  // Start Receiving
  ssize_t chunk_size;
  long received_size = 0;
  while (received_size < file_size &&
         (chunk_size = recv(recfd, buffer, sizeof(buffer), 0)) > 0) {
    if (chunk_size == -1) {
      fprintf(stderr, "(%d) can't receive packet", recfd);
      perror("");
      fclose(fd);
      return;
    }
    if (received_size + chunk_size > file_size) {
      fwrite(buffer, 1, file_size - received_size, fd);
      received_size += file_size - received_size;
    } else {
      fwrite(buffer, 1, chunk_size, fd);
      received_size += chunk_size;
    }
  }
  fprintf(stderr, "(%d) Saved: %s\n", recfd, target_file);
  fclose(fd);
}

void server_process(int recfd, char *full_command, char **current_path) {
  // Prepare
  char *delim = " "; //dấu cách phân định command
  // strtok: Chia 1 chuỗi dài thành các chuỗi nhỏ được phân biệt riêng rẽ bởi các ký tự đặc biệt được chọn.
  char *command = strtok(full_command, delim); //lấy phần đầu của command (ls,cd,...)
  char *context = strtok(NULL, delim); //phần sau của command
  char *response = malloc(sizeof(char) * 1024);

  // Process
  if (begin_with(command, "ls")) {
    server_ls(recfd, response, current_path);
    respond(recfd, response);
  } else if (begin_with(command, "cd")) {
    server_cd(recfd, context, response, current_path);
    respond(recfd, response);
  } else if (begin_with(command, "download")) {
    server_download(recfd, context, current_path);
  } else if (begin_with(command, "upload")) {
    server_upload(recfd, context, current_path);
  } else {
    strcpy(response, "No such command: ");
    strcat(response, command);
    // gửi thông điệp (phản hồi) lại client
    respond(recfd, response);
  }

  // Cleanup
  free(response);
}

void *Accept_Client(void *recfd) {

  // Initialize Buffer, Response, FDs
  int buffer_size = 1024;
  
  char username[MAX],pass[MAX], *reply;
  int bytes_sent, bytes_received;
  char filename[] = "account.txt";
  // buffer: thông điệp trao đổi (command)
  char *buffer = malloc(sizeof(char) * buffer_size);
  // current_path: đường dẫn hiện tại
  char *current_path = malloc(sizeof(char) * 2);
  // strcpy(current_path, "./test");     
  node_a *found;
	node_a *account_list = loadData(filename);
  // Read-Evaluate-Print Loop
  // vòng lặp đọc-thực thi-in
  int login = 0;
  while (true) {
    // login
    if (0 == login)
    {
      if (0 >= (bytes_received = recv((int)recfd,username,MAX-1,0))){
        printf("Connection closed\n");
        break;
      }
      username[bytes_received] = '\0';

      // check username exist
      if (found = findNode(account_list,username))
      {
        if (found->status == 1)
        {
          reply = "1";
        }
        else reply = "2";      
      }
      else
      {
        reply = "0";
      }

      if (0>=(bytes_sent = send((int)recfd,reply,strlen(reply),0)))
      {
        printf("\Connection closed\n");
        break;
      }
      
      int count = 0;

      while (1)
      {
        // receive pass
        memset(pass,'\0',MAX);
        if (0 >= (bytes_received = recv((int)recfd,pass,MAX-1,0)))
        {
          printf("\nConnection closed\n");
          break;
        }
        pass[bytes_received] = '\0';

        // validate pass
        if (0 == strcmp(found->pass,pass))
        {
          reply = "1";
          login = 1;
        }
        else {
          count++;
          if (count == 3)
          {
            reply = "2";
            found->status = 0;
          }
          else reply="0";  
        }

        if (0>=(bytes_sent = send((int)recfd,reply,strlen(reply),0)));
        {
          printf("\n%s is connected\n",username);
          break;
        }  
      }
      saveData(account_list,"account.txt");
      // cấp thư mục
    }
    
    
    strcpy(current_path, "./test");
      
    // Recieve message
    // in ra VD: (4)terminated
    if ((recv((int)recfd, buffer, buffer_size, 0)) < 1) {
      fprintf(stderr, "(%s) Terminated", username);
      perror("");
      close((int)recfd);
      break;
    }

    // Evaluate
    // in ra command đã nhận
    printf("(%s) Command: %s\n", username, buffer);
    // thực thi command
    server_process((int)recfd, buffer, &current_path);
  }

  // Clean Up
  free(buffer);
  free(current_path);
  pthread_exit(NULL);
}

int main(int argc, const char *argv[]) {

  // Initialize Addresses, Threads
  struct sockaddr_in server_addr, client_addr;
  socklen_t client_addr_length = sizeof(client_addr);
  bzero(&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(PortNumber);
  server_addr.sin_addr.s_addr = INADDR_ANY;

  // Create Socket
  if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
    // perror("Create socket error");
    // exit(errno);
    perror("\nError: ");
		return 0;
  }

  // Bind Socket to Address
  if ((bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr))) == -1) {
    perror("Binding error");
    // close(sock);
    // exit(errno);
    return 0;
  }

  // Start Listening
  if ((listen(sock, MaxClient)) == -1) {
    perror("\nError:");
    // close(sock);s
    // exit(errno);
    return 0;
  }

  // Registered handler
  // signal(SIGINT, handle_sigint);

  // Start Accepting
  printf("Start Listening\n");
  while (1) {
    // int *recfd = malloc(sizeof(int));
    void *recfd = accept(sock, (struct sockaddr *)&client_addr, &client_addr_length);
    if ((int)recfd == -1) {
      perror("Accept error");
      continue;
    }

    // if ((*recfd = accept(sock, (struct sockaddr *)&client_addr, &client_addr_length)) == -1)
    // {
    //   perror("\nError:");
    // }
    
    printf("(%d) Accepted\n", recfd);
    pthread_t thread;
    if (pthread_create(&thread, NULL, Accept_Client, recfd)) {
      printf("(%d) Create thread error\n", (int)recfd);
      continue;
    }
  }
  close(sock);
  return 0;
}
