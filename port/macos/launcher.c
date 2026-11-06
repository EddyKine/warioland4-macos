#include <mach-o/dyld.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
int main(int argc,char **argv) {
 char exe[PATH_MAX],root[PATH_MAX],game[PATH_MAX],rom[PATH_MAX],save[PATH_MAX],path[PATH_MAX];
 uint32_t size=sizeof(exe);
 if(_NSGetExecutablePath(exe,&size)||!realpath(exe,root))return 1;
 strcpy(exe,root);
 char *slash=strrchr(exe,'/');if(!slash)return 1;*slash=0;
 if(snprintf(game,sizeof(game),"%s/wl4",exe)>=(int)sizeof(game))return 1;
 for(int i=0;i<4;i++){slash=strrchr(root,'/');if(!slash)return 1;*slash=0;}
 snprintf(path,sizeof(path),"%s/saves",root);mkdir(path,0700);
 snprintf(path,sizeof(path),"%s/.work/tmp",root);setenv("TMPDIR",path,1);
 snprintf(path,sizeof(path),"%s/.work/cache",root);setenv("XDG_CACHE_HOME",path,1);
 snprintf(path,sizeof(path),"%s/settings.cfg",root);
 if(!getenv("WL4_SETTINGS_FILE"))setenv("WL4_SETTINGS_FILE",path,1);
 #ifdef WL4_AUDIO_TRIAL
 snprintf(path,sizeof(path),"%s/wl4-audio-test.log",root);
#else
 snprintf(path,sizeof(path),"%s/wl4-last-run.log",root);
#endif
 const char *log=getenv("WL4_LOG_FILE");
 if(!log){
  char folder[PATH_MAX],archive[PATH_MAX];
  snprintf(folder,sizeof(folder),"%s/logs",root);mkdir(folder,0700);
  snprintf(archive,sizeof(archive),"%s/wl4-before-%lld-%ld.log",folder,(long long)time(NULL),(long)getpid());
  if(access(archive,F_OK)!=0)rename(path,archive);
  log=path;
 }
 int fd=open(log,O_WRONLY|O_CREAT|O_TRUNC,0600);
 if(fd>=0){dup2(fd,STDOUT_FILENO);dup2(fd,STDERR_FILENO);close(fd);}
 snprintf(rom,sizeof(rom),"%s/.work/rom/baserom_us.gba",root);
#ifdef WL4_AUDIO_TRIAL
 snprintf(save,sizeof(save),"%s/saves/wl4-audio-test.sav",root);
#else
 snprintf(save,sizeof(save),"%s/saves/wl4.sav",root);
#endif
 if(chdir(root)){perror("Cannot open external SSD directory");return 1;}
 char **args=calloc((size_t)argc+5,sizeof(char *));if(!args)return 1;
 args[0]=game;args[1]=rom;args[2]="--save";args[3]=save;
 for(int i=1;i<argc;i++)args[i+3]=argv[i];
 execv(game,args);perror("Cannot start Wario Land 4");return 1;
}
