#include <arpa/inet.h>  // htons(), inet_addr()
#include <dirent.h>     // opendir(), readdir(), closedir()
#include <errno.h>      // errno
#include <netinet/in.h> // inet_addr(), bind()
#include <signal.h>     // signal()
#include <stdbool.h>    // bool
#include <stdio.h> 
#include <stdlib.h>     // strtol()
#include <string.h>     // bzero(), strlen(), strcmp(), strcpy(), strtok(), strrchr(), memcmp()
#include <sys/socket.h> // socket(), inet_addr(), bind(), listen(), accept(), recv(), send()
#include <sys/types.h>  // socket()
#include <unistd.h>     // close()
#include <ctype.h>
#include <sys/stat.h>   //mkdir()
#include <sys/wait.h>   //waitpid();
#include <libgen.h>

#include "account.h"

#define MaxClient 20
#define MAX 100

 //listenfd for server;
int is_regular_file(const char *path)
{
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}
int dem(char *s,char t);
void sig_chld(int signo);
int begin_with(const char *str, const char *pre);
int respond(int recfd, char response[]);
void server_ls(int recfd, char *response, char **current_path);
void server_cd(int recfd, char *open_dir, char *response, char **current_path);
void server_download(int recfd, char *target_file, char **current_path);
void server_upload(int recfd, char *target_file, char **current_path);
void server_mkdir(int recfd,char *new_dir, char *respond, char **current_path);
void server_process(int recfd, char *full_command, char **current_path);
void server_rm(int recfd, char *full_command, char *target_file, char **current_path);

int main(int argc, const char *argv[]) {
  // check argument
 	if(argc != 2){
 		printf("Invalid argument\n\n");
 		return 0;
 	}

  char username[MAX],pass[MAX],folder[MAX], *reply;
  int bytes_sent, bytes_received;
  char filename[] = "account.txt";
  char str[MAX];

  // Initialize Addresses
  int sock;
  struct sockaddr_in server_addr, client_addr;
  socklen_t client_addr_length = sizeof(client_addr);
  bzero(&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(atoi(argv[1]));
  server_addr.sin_addr.s_addr = INADDR_ANY;
  int pid;

  node_a *found;
	node_a *account_list = loadData(filename);

  // menu
  int chon = 0;
  do
  {
      // system("clear");
      printf("\n============MENU===========\n");
      printf("|1. Show all clients       |\n");
      printf("|2. Create a client        |\n");
      printf("|3. Update clients         |\n");
      printf("|4. Start server           |\n");
      printf("|5. Delete client          |\n");
      printf("|6. Exit                   |\n");
      printf("===========================\n");
      printf("Enter your choice: ");
      scanf("%d", &chon);
      switch(chon)
      {
          case 1:
            // show all clients
            printf("\n");
            printf("Username\tPass\t\tFolder\n");
            for(node_a *p = account_list; p != NULL; p = p->next){
                printf("%s\t\t%s\t\t%s\n", p->username, p->pass, p->folder);
            }
            printf("\n");  
            break;
          case 2:
            // create client
            // printf("\n");
            while (getchar() != '\n');
            printf("New Client Username: ");
            fgets(username,MAX,stdin);
            username[strcspn(username,"\n")] = '\0';
            while (findNode(account_list,username) != NULL) {
              printf("Client existed.Please try again\n");
              printf("New Client Username: ");
              fgets(username,MAX,stdin);
              username[strcspn(username,"\n")] = '\0';
            }        
            
            printf("New Client Password: ");
            fgets(pass,MAX,stdin);
            pass[strcspn(pass,"\n")] = '\0';

            int c = 0;
            do
            {
              printf("New Client Folder Name: ");
              fgets(folder,MAX,stdin);
              folder[strcspn(folder,"\n")] = '\0';

              errno = 0;
              int ret = mkdir(folder, S_IRWXU);
              if (ret == -1) {
                switch (errno) {
                  case EACCES :
                      printf("The root directory does not allow write. ");
                      break;
                  case EEXIST:
                      printf("Folder %s already exists. \n%s used for client.",folder, folder);
                      c = 1;
                      break;
                  case ENAMETOOLONG:
                      printf("Pathname is too long");
                      break;
                  default:
                      printf("mkdir");
                      break;
                }
              }
              else
              {
                printf("Created: %s\n", folder);
                printf("Folder %s is created",folder);
                c = 1;
              }
            } while (c == 0);
            
            account_list = AddTail(account_list,username,pass,folder);
            saveData(account_list,filename);
            
            printf("\n");
            break;
          case 3:
            while (getchar() != '\n');
            printf("Client Username: ");
            fgets(username,MAX,stdin);
            username[strcspn(username,"\n")] = '\0'; 
            node_a *found = findNode(account_list,username);
            if (found == NULL)
            {
              printf("Username \"%s\" not exist!\n",username);
            }
            else
            {
              account_list = updateNode(account_list,found);
              saveData(account_list,filename);
            } 
            break;
          case 4:
            // start server
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
              return 0;
            }
            signal(SIGCHLD, sig_chld);

            // Start Accepting
            printf("Start Listening\n"); 
                       
            while (1) {                                  
              int recfd;
              recfd = accept(sock, (struct sockaddr *)&client_addr, &client_addr_length);
              if (recfd == -1) {
                perror("Accept error");
                continue;
              }

              pid = fork();
              if (pid < 0)
              {
                perror("Error: ");
                return 1;
              }
              
              if (pid == 0)
              {   
                // Initialize Buffer, Response, FDs
                int buffer_size = 1024;
                // buffer: thông điệp trao đổi (command)
                char *buffer = malloc(sizeof(char) * buffer_size);
                // current_path: đường dẫn hiện tại
                char *current_path = malloc(sizeof(char) * 2);   
                
                int login = 0,path = 0;
                strcpy(current_path,".");
                while (login == 0) {
                  if (0 >= (bytes_received = recv(recfd,username,MAX-1,0))){
                    printf("Connection closed\n");
                    break;
                  }
                  username[bytes_received] = '\0';

                  // check username exist
                  if (found = findNode(account_list,username))
                  { 
                    reply = "1";  
                    if (0>=(bytes_sent = send(recfd,reply,strlen(reply),0)))
                    {
                      printf("Connection closed\n");
                      break;
                    } 
                  }
                  else
                  {
                    reply = "0";
                    if (0>=(bytes_sent = send(recfd,reply,strlen(reply),0)))
                    {
                      printf("Connection closed\n");
                      break;
                    }
                    continue;
                  }

                  

                  while (1)
                  {
                    // receive pass
                    memset(pass,'\0',MAX);
                    if (0 >= (bytes_received = recv(recfd,pass,MAX-1,0)))
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
                      reply="0";  
                    }

                    if (0 >= (bytes_sent = send(recfd,reply,strlen(reply),0)));
                    {
                      printf("\n%s is try to connect\n",username);
                      break;
                    }  
                  }
                } 
                
                while (path == 0) {
                  /* code */                 
                  strcat(current_path,"/");
                  strcat(current_path,found->folder);
                  printf("%s in %s\n",username,current_path);
                  respond((int)recfd,current_path);
                  path = 1;
                }

                // Read-Evaluate-Print Loop
                // vòng lặp đọc-thực thi-in
                while (true) {
                  // Recieve message
                  // in ra VD: (4)terminated
                  if ((recv(recfd, buffer, buffer_size, 0)) < 1) {
                    fprintf(stderr, "(%s) Terminated\n", username);
                    // perror("");
                    close(recfd);
                    break;
                  }
                  // Evaluate
                  // in ra command đã nhận
                  printf("(%s) Command: %s\n", username, buffer);
                  // thực thi command
                  server_process(recfd, buffer, &current_path);
                }

                // Clean Up
                free(buffer);
                free(current_path);
                // close(recfd);
              }
              else
              {
                close(recfd);
              }  
            }
            
            close(sock);
            break;
          case 5:
              while (getchar() != '\n');
              printf("Client Username: ");
              fgets(username,MAX,stdin);
              username[strcspn(username,"\n")] = '\0';
              if (findNode(account_list,username) == NULL)
              {
                printf("Username \"%s\" not exist!\n",username);
              }
              else if (account_list == findNode(account_list,username))
              {
                account_list = deleteHead(account_list);
                printf("Client \"%s\" deleted",username);
              }
              else
              {
                account_list = deleteAt(account_list,username);
                printf("Client \"%s\" deleted",username);
              }
              break;
          case 6:
              
              break;
          default:
              printf("Ban chon sai. Moi ban chon lai MENU!\n");
              break;
      }
  }while(chon!=6); 
  free(account_list); 
  return 0;
}


// xem command str có bắt đầu bằng xâu pre (VD begin_with(command,"ls") trả về 1 nếu command bắt đầu bằng ls)
int begin_with(const char *str, const char *pre) {
  size_t lenpre = strlen(pre), lenstr = strlen(str);
  return lenstr < lenpre ? 0 : memcmp(pre, str, lenpre) == 0;
}

// gửi thông điệp (phản hồi) lại client
int respond(int recfd, char response[]) {
  if ((send(recfd, response, strlen(response) + 1, 0)) == -1) {
    fprintf(stderr, "Can't send packet\n");
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
    fprintf(stderr, "Can't open %s", *current_path); //remain in server
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
    fprintf(stderr, "Directory Close Error");
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
    if (dem(*current_path,'/') == 1) {
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
  
  if (is_regular_file(full_path) == 0)
  {
    respond(recfd,"@Cannot download a folder or file not existed");
    fprintf(stderr, "%s is a folder or not existed\n",full_path);
    // fclose(fd);
    return;
  }
  // Initialize File Descriptor, Buffer
  FILE *fd;
  if ((fd = fopen(full_path, "rb")) == NULL) {
    respond(recfd, "@file open error");
    fprintf(stderr, "Can't open %s", full_path);
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
    fprintf(stderr, "Can't send packet");
    perror("");
    fclose(fd);
    return;
  }
  fseek(fd, 0L, SEEK_SET); 

  // Wait for client to be ready
  ssize_t byte_received = recv(recfd, buffer, sizeof(buffer), 0);
  if (byte_received == -1) {
    fprintf(stderr, "Can't receive packet");
    perror("");
    fclose(fd);
    return;
  }

  // Start Transmission
  while ((chunk_size = fread(buffer, 1, sizeof(buffer), fd)) > 0) {
    ssize_t byte_sent = send(recfd, buffer, chunk_size, 0);
    if (byte_sent == -1) {
      fprintf(stderr, "Can't send packet");
      perror("");
      fclose(fd);
      return;
    }
  }
  printf("Transmited: %s\n", target_file);
  fclose(fd);
}

void server_upload(int recfd, char *target_file, char **current_path) { 
  // Build Path
  // char file_name[MAX];
  char *file_name = basename(target_file);
  char *full_path = malloc(strlen(*current_path) + strlen(file_name) + 2);
  strcpy(full_path, *current_path);
  strcat(full_path, "/");
  strcat(full_path, file_name);
  if( access(full_path, F_OK ) == 0 ) {
    // file exists
    fprintf(stderr, "File already exists\n");
    respond(recfd,"@File already exists");
    free(full_path);
    return;
  } 
  
  // Initialize File Descriptor
  FILE *fd;
  if ((fd = fopen(full_path, "wb")) == NULL) {
    respond(recfd, "@file open error");
    fprintf(stderr, "Can't open %s",*current_path);
    perror("");
    return;
  }

  // Retrieve File Size
  char buffer[1024];
  strcpy(buffer, "size?");
  ssize_t byte_sent = send(recfd, buffer, strlen(buffer) + 1, 0);
  if (byte_sent == -1) {
    fprintf(stderr, "Can't send packet");
    perror("");
    fclose(fd);
    return;
  }
  ssize_t byte_received = recv(recfd, buffer, sizeof(buffer), 0);
  if (byte_received == -1) {
    fprintf(stderr, "Can't receive packet");
    perror("");
    fclose(fd);
    return;
  }
  long file_size = strtol(buffer, NULL, 0);  //strol => coveert string to long int

  // Notify client to start transmission
  strcpy(buffer, "ready");
  byte_sent = send(recfd, buffer, strlen(buffer) + 1, 0);
  if (byte_sent == -1) {
    fprintf(stderr, "Can't send packet");
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
      fprintf(stderr, "Can't receive packet");
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
  free(full_path);
  fprintf(stderr, "Saved: %s\n", file_name);
  fclose(fd);
}

void server_rm(int recfd, char *target_file, char *response, char **current_path) {
  // if (target_file == NULL)
  // {
  //   strcpy(response,"@no file name given");
  //   return;
  // } 

  char *full_path = malloc(strlen(*current_path) + strlen(target_file) + 2);
  strcpy(full_path, *current_path);
  strcat(full_path, "/");
  strcat(full_path, target_file);
  
  int ret = remove(full_path);
  if(ret != 0) {
    strcpy(response,"@Error: unable to delete the file/folder");
    free(full_path);
    perror("Error");
    return;
  } else {
    printf("File deleted successfully\n");
    strcpy(response,"@File deleted successfully");   
  }
  free(full_path);
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
  } else if (begin_with(command,"mkdir")) {
    server_mkdir(recfd, context, response, current_path);
    respond(recfd,response);
  } else if (begin_with(command,"rm")){
    server_rm(recfd,context,response,current_path);
    respond(recfd,response);
  }
  else {
    strcpy(response, "No such command: ");
    strcat(response, command);
    // gửi thông điệp (phản hồi) lại client
    respond(recfd, response);
  }

  // Cleanup
  free(response);
}

void server_mkdir(int recfd,char *new_dir, char *response, char **current_path){
    // Handle empty arg and . and ..
  // xử lý đối số trống . và .. 
  if (new_dir == NULL)
  {
    strcpy(response,"@no directory name given");
    return;
  } else if (strcmp(new_dir,".") == 0 || strcmp(new_dir,"..") == 0)
  {
    strcpy(response,"@Wrong name format");
    return;
  }

  char *new_path = malloc(strlen(*current_path) + strlen(new_dir) + 2);
  strcpy(new_path, *current_path);
  strcat(new_path, "/");
  strcat(new_path, new_dir);

  errno = 0;
  int ret = mkdir(new_path, S_IRWXU);
  if (ret == -1) {
      switch (errno) {
          case EACCES :
              strcpy(response,"@The parent directory does not allow write");
              free(new_path);
              return;
          case EEXIST:
              strcpy(response,"@pathname already exists");
              free(new_path);
              return;
          case ENAMETOOLONG:
              strcpy(response,"@pathname is too long");
              free(new_path);
              return;
          default:
              strcpy(response,"@mkdir");
              free(new_path);
              return;
      }
  }
  else
  {
    fprintf(stderr, "Created: %s\n", new_dir);
    strcpy(response,"@Folder is created");
  }
  free(new_path);
}

void sig_chld(int signo){
	pid_t pid;
	int stat;
	
	/* Wait the child process terminate */
	while((pid = waitpid(-1, &stat, WNOHANG))>0)
		printf("\nChild %d terminated\n",pid);
}

int dem(char *s,char t)
{
 int dem=0;
 for(int i=0;i<=strlen(s);i++)
 {
 if(s[i]==t) dem=dem+1;  
 }
 return dem;
}