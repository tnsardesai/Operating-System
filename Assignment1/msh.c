/*
 * Name: Tanmay Sardesai 
 * ID #: 1001094616
 * Programming Assignment 1
 * Description: Maverick Shell.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/types.h>


/*
 * This is the most important function. It is called in main inside a loop everytime it passes.
 * This exits out of the loop only when "exit" or "quit" is entered in msh.
 */
int shell(int* show_pid);

/*
 * This function is used to read input entered by the user. It uses malloc for allocation and 
 * fgets for storing input from stdin
 */
char* read_line();

/*
 * This function is used to tokenize the input. It uses strtok to create an array of strings for
 * the given input.
 */
char** tokenize(char* input);

/*
 * This function is defined to concatenate two strings. It is used to join path with first string
 * in token in exec function.
 */
char* concat(const char *s1, const char *s2);

/*
 * Calls execv on the path and tokens provided. It used execv because we already have an array of 
 * tokens.
 */
int exec_path(char** token,char* path);

/*
 * This functions performs checks and then uses chdir(...) to cd.
 */
void change_dir(char** token);

/*
 * Used to catch ctrl-C and ctrl-Z as we dont want to exit.
 */
static void handle_signal (int sig);


static void handle_signal (int sig ){
  printf("\n");
}

int main(int argc, char** argv) {

  /*
   * The next few lines are used to do handle signals.
   */
  struct sigaction act;
  memset (&act, '\0', sizeof(act));
  act.sa_handler = &handle_signal;

  if (sigaction(SIGINT , &act, NULL) < 0)
  {
    perror("sigaction:");
    return 0;
  }

  if (sigaction(SIGTSTP , &act, NULL) < 0)
  {
    perror("sigaction:");
    return 0;
  }

  /*
   * Declaring and array for show_pid and then memset to initialize everything to 0;
   */
  int *show_pid = malloc (10*sizeof(int));
  memset(show_pid, 0, 10*sizeof(int));
  int run = 1;
  while(run){
   run = shell(show_pid);
  }

  free(show_pid);
  return 0;
}

int shell(int* show_pid){

  //Calculate the length of show_pid. Limiting it at 10 as we only want to print at most 10.
  int length=0;
  while(show_pid[length]!=0 && length<10 ){
    length=length+1;
  }

  printf("msh> ");

  char *input;
  char **token;
  int found=1;

  input = read_line();
  // At exit or quit return such that it exits.
  if(!strcmp(input,"exit\n") || !strcmp(input,"quit\n"))
    return 0;

  // This is the code for running showpid. Since we want most recent print the last pid first.
  if(!strcmp(input,"showpid\n")){
    int counter=0;
    for(counter=length-1; counter>=0; counter--){
      printf("%d\n",show_pid[counter]);
    }
    return 1;
  }

  token = tokenize(input);

  if (token[0]=='\0'){
    return 1;
  }
  if(!strcmp(token[0],"cd")){
    change_dir(token);
    free(input);
    free(token);
    return 1;
  }

  /* Check for all the paths defined in the array below. If found then store the pid of the
   * process executed in showpid. If length is at 10 then move things to the left and then write 
   * found to showpid[9];
   */
  char *paths[4] = {"./","/usr/local/bin/","/usr/bin/","/bin/"};
  int i=0;
  while(i<4){
    found = exec_path(token,paths[i]);
    if(!(found == 0)){
      if (length == 10){
        int counter = 0;
        for(counter = 0; counter<length-1;counter++){
          show_pid[counter] = show_pid[counter+1];
        }
        length = 9;
      }
      show_pid[length] = found;
      break;
    }
    else{
      i++;
    }
  }

  // If not showpid, not cd and not found in all 4 paths then command not found.
  if(i==4){
    printf("%s: Command not found.\n", token[0]);
  }
  free(input);
  free(token);

  return 1;
}

char* read_line(){
  char *result = malloc (100*sizeof(char));
  if (!result){
    printf("ERROR IN ALLOCATING MEMORY FOR MALLOC IN READ_LINE.\n");
    exit(1);
  }
  // useing fgets to scan and store in result which is returned. 
  fgets(result,100,stdin);
  return result;
}

char** tokenize(char* input){
  int i = 0;
  char **token = malloc(6*sizeof(char*));
  if (!token){
    printf("ERROR IN ALLOCATING MEMORY FOR MALLOC IN TOKENIZE.\n");
    exit(1);
  }
  // Using strtok to make an array of tokens from input. 
  token[i]= strtok(input, " \t\n");
  while(token[i] != NULL){
    i++;
    token[i] = strtok(NULL, " \t\n");
  }

  return token;
}

int exec_path(char** token,char* path){

  pid_t child_pid;
  int status;
  int found=0;
  char* s = concat(path,token[0]); // Concat the path with token[0] to pass to execv

  child_pid = fork();

  if (child_pid == 0){ // runs in child
    if(execv(s,token) == -1){
    }
    exit(EXIT_FAILURE);
  }
  else if (child_pid < 0){
    printf("ERROR IN EXEC. FORK FAILED.\n");
  }
  else {
    waitpid(child_pid, &status, 0 );
    if (WEXITSTATUS(status)==1){ // This is returns 1 only when command not found.
      found = 0;
    }
    else {
      found = child_pid; // return child_pid for show_pid to store.
    }
  }
  return found;
}

char* concat(const char *s1, const char *s2){
  char *result = malloc(strlen(s1)+strlen(s2)+1);
  strcpy(result, s1);
  strcat(result, s2);
  return result;
}

void change_dir(char** token){
  if(token[1]==NULL){ // checks for cd ..
    printf("cd: %s: No such file or directory.\n",token[1]);
  }
  else{  // uses chdir
    if(chdir(token[1]) !=0){
      printf("THERE IS SOME ERROR IN CHDIR. IT FAILED.");
    }
  }
}

