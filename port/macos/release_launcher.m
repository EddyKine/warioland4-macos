#import <Cocoa/Cocoa.h>
#import <CommonCrypto/CommonDigest.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static BOOL headless;
static void stop(NSString *message) {
 fprintf(stderr,"%s\n",message.UTF8String);
 if(!headless){[NSApplication sharedApplication];[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];[NSApp finishLaunching];[NSApp activateIgnoringOtherApps:YES];NSAlert *a=[NSAlert new];a.messageText=@"No se pudo iniciar el juego";a.informativeText=message;[a runModal];}
 exit(1);
}
static BOOL valid(NSString *path, BOOL bios) {
 NSData *d=[NSData dataWithContentsOfFile:path options:NSDataReadingMappedIfSafe error:nil];
 #ifdef YOSHI_RELEASE
 if(d.length!=(bios?16384:4194304))return NO;
 unsigned char digest[CC_SHA1_DIGEST_LENGTH];CC_SHA1(d.bytes,(CC_LONG)d.length,digest);
 NSMutableString *hash=[NSMutableString new];for(int i=0;i<CC_SHA1_DIGEST_LENGTH;i++)[hash appendFormat:@"%02x",digest[i]];
 return [hash isEqualToString:bios?@"300c20df6731a33952ded8c436f7f186d25d3492":@"7352d2bd064d9ebaec579e264228aa21c7345b80"];
 #else
 if(d.length!=8388608)return NO;
 unsigned char digest[CC_MD5_DIGEST_LENGTH];CC_MD5(d.bytes,(CC_LONG)d.length,digest);
 NSMutableString *hash=[NSMutableString new];for(int i=0;i<CC_MD5_DIGEST_LENGTH;i++)[hash appendFormat:@"%02x",digest[i]];
 return [hash isEqualToString:@"5fe47355a33e3fabec2a1607af88a404"];
 #endif
}
static NSString *choose(NSString *data, BOOL bios, BOOL force) {
 NSString *stored=[data stringByAppendingPathComponent:bios?@"bios-path.txt":@"rom-path.txt"];
 const char *override=getenv(bios?"PORT_BIOS":"PORT_ROM");
 NSString *path=override?[@(override) stringByStandardizingPath]:[NSString stringWithContentsOfFile:stored encoding:NSUTF8StringEncoding error:nil];
 if(!force&&path&&valid(path,bios))return path;
 if(headless)stop(bios?@"BIOS ausente o incompatible. Usa PORT_BIOS con una ruta válida.":@"ROM ausente o incompatible. Usa PORT_ROM con una ruta válida.");
 [NSApplication sharedApplication];[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];[NSApp finishLaunching];[NSApp activateIgnoringOtherApps:YES];
 for(;;){
  NSOpenPanel *panel=[NSOpenPanel openPanel];panel.canChooseDirectories=NO;panel.allowsMultipleSelection=NO;
  panel.title=bios?@"Selecciona tu BIOS GBA (16 KiB)":@"Selecciona tu ROM compatible del juego";
  panel.message=bios?@"Debes aportar tu propia BIOS GBA. No está incluida.":@"Debes aportar tu propia ROM USA/Europa compatible. No está incluida.";
  if([panel runModal]!=NSModalResponseOK)exit(0);
  path=panel.URL.path;
  if(valid(path,bios)){NSError *e=nil;if(![path writeToFile:stored atomically:YES encoding:NSUTF8StringEncoding error:&e])stop(e.localizedDescription);return path;}
  NSAlert *a=[NSAlert new];a.messageText=@"Archivo incompatible";a.informativeText=bios?@"Se requiere una BIOS GBA de 16.384 bytes con SHA1 300c20df6731a33952ded8c436f7f186d25d3492.":@"El tamaño o hash de la ROM no coincide con la versión admitida. Consulta LEEME.txt.";[a runModal];
 }
}
int main(int argc,char **argv){@autoreleasepool{
 BOOL force=NO;for(int i=1;i<argc;i++){if(!strcmp(argv[i],"--headless"))headless=YES;if(!strcmp(argv[i],"--choose-files"))force=YES;}
 #ifdef YOSHI_RELEASE
 NSString *name=@"Yoshi Island",*runtimeName=@"sma3-runtime";
 #else
 NSString *name=@"Wario Land 4",*runtimeName=@"wl4";
 #endif
 const char *override=getenv("PORT_DATA_DIR");NSString *data=override?@(override):[[NSHomeDirectory() stringByAppendingPathComponent:@"Library/Application Support/EddyKine"] stringByAppendingPathComponent:name];
 if(!data.isAbsolutePath)stop(@"PORT_DATA_DIR debe ser una ruta absoluta.");
 NSFileManager *fm=[NSFileManager defaultManager];NSError *error=nil;
 for(NSString *sub in @[@"",@"saves",@"states",@"menu-profile",@"logs",@"cache",@"tmp"]){if(![fm createDirectoryAtPath:[data stringByAppendingPathComponent:sub] withIntermediateDirectories:YES attributes:nil error:&error])stop(error.localizedDescription);}
 NSString *runtime=[[[NSBundle mainBundle] executablePath].stringByDeletingLastPathComponent stringByAppendingPathComponent:runtimeName];
 if(![fm isExecutableFileAtPath:runtime])stop(@"Falta el ejecutable del juego dentro de la aplicación.");
 if(!headless&&([NSEvent modifierFlags]&NSEventModifierFlagOption))force=YES;
 NSString *rom=choose(data,NO,force);
 #ifdef YOSHI_RELEASE
 NSString *bios=choose(data,YES,force);
 #endif
 if(chdir(data.fileSystemRepresentation))stop(@"No se pudo abrir la carpeta de datos.");
 setenv("TMPDIR",[data stringByAppendingPathComponent:@"tmp"].fileSystemRepresentation,1);
 setenv("XDG_CACHE_HOME",[data stringByAppendingPathComponent:@"cache"].fileSystemRepresentation,1);
 setenv("WL4_SETTINGS_FILE",[data stringByAppendingPathComponent:@"settings.cfg"].fileSystemRepresentation,1);
 setenv("GBARECOMP_HEAL_CACHE",[data stringByAppendingPathComponent:@"cache"].fileSystemRepresentation,1);
 setenv("GBARECOMP_SELFHEAL_RECOMPILE","0",1);setenv("GBARECOMP_DIAGNOSTICS","0",1);
 unsetenv("GBARECOMP_INPUT_RECORD");unsetenv("GBARECOMP_INPUT_REPLAY");unsetenv("GBARECOMP_DEMO_INPUT");
 NSString *log=[data stringByAppendingPathComponent:@"logs/last-run.log"];
 int fd=open(log.fileSystemRepresentation,O_WRONLY|O_CREAT|O_TRUNC,0600);if(fd<0)stop(@"No se pudo crear el registro de ejecución.");dup2(fd,1);dup2(fd,2);close(fd);
 NSMutableArray<NSString *> *args=[NSMutableArray arrayWithObject:[data stringByAppendingPathComponent:name]];
 #ifdef YOSHI_RELEASE
 [args addObjectsFromArray:@[@"--rom",rom,@"--bios",bios,@"--save-path",[data stringByAppendingPathComponent:@"saves/game.sav"],@"--state-dir",[data stringByAppendingPathComponent:@"states"]]];
 if(!headless)[args addObject:@"--window"];
 #else
 [args addObjectsFromArray:@[rom,@"--save",[data stringByAppendingPathComponent:@"saves/game.sav"]]];
 #endif
 for(int i=1;i<argc;i++)if(strcmp(argv[i],"--headless")&&strcmp(argv[i],"--choose-files")&&strncmp(argv[i],"-psn_",5))[args addObject:@(argv[i])];
 char **a=calloc(args.count+1,sizeof(char*));if(!a)stop(@"No hay memoria para iniciar.");for(NSUInteger i=0;i<args.count;i++)a[i]=strdup(args[i].UTF8String);
 execv(runtime.fileSystemRepresentation,a);stop(@"No se pudo ejecutar el juego.");
}}
