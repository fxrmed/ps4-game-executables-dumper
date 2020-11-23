#include "main.h"
#include "kernel_utils.h"

int nthread_run;
char notify_buf[512];
configuration config;

void* nthread_func(void* arg) {
  time_t t1, t2;
  t1 = 0;
  while (nthread_run) {
    if (notify_buf[0]) {
      t2 = time(NULL);
      if ((t2 - t1) >= config.notify) {
        t1 = t2;
        printf_notification(notify_buf);
      }
    } else {
      t1 = 0;
    }
    sceKernelSleep(1);
  }

  return NULL;
}

#define ENOENT           2      /* No such file or directory */

int isDirectory(char *path)
{
	DIR* dir = opendir(path);

	if (dir) 
	{
    	closedir(dir);
    	return 1;
	} 
	else if (ENOENT == errno) 
	{
	    return 0; //directory doesnt exist
	} 
	else 
	{
	    return 0; //uhh, some other random error?
	}
}

int isFile(char *path)
{
	FILE *file;
    if ((file = fopen(path, "r")))
    {
        fclose(file);
        return 1;
    }
    return 0;
}

int ifSprxOrEboot(char *path)
{
	if(strstr(path, "eboot.bin") != NULL) 
	{
		return 1;
	}
	else if(strstr(path, ".prx") != NULL) 
	{
		return 1;
	}
	else if(strstr(path, ".sprx") != NULL) 
	{
		return 1;
	}
	else 
	{
		return 0;
	}
}

void decrypt_and_dump_execs_in_dir(char *sourcedir, char* destdir)
{
    DIR *dir;
    struct dirent *dp;
    char src_path[1024], dst_path[1024];
    int isEmptyFolder = 1;

    dir = opendir(sourcedir);
    if (!dir)
    {
    	printf_notification("Couldn't open dir?");
        return;
    }

    mkdir(destdir, 0777);

    while ((dp = readdir(dir)) != NULL)
    {
        if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
        {
            //because yes
        }
        else
        {
            sprintf(src_path, "%s/%s", sourcedir, dp->d_name);
            sprintf(dst_path, "%s/%s", destdir, dp->d_name);

            //stat doesn't work for some reason (st_mode is always zero), prolly because of SCE's hokus pocus stuff
            if (isDirectory(src_path))
                {
                    decrypt_and_dump_execs_in_dir(src_path, dst_path);
                }
            else if (isFile(src_path))
                {
                    if (is_self(src_path) || ifSprxOrEboot(src_path))
                    {
                    	isEmptyFolder = 0;
                    	printf_notification("Decrypting and dumping:\n%s", dp->d_name);
                        decrypt_and_dump_self(src_path, dst_path);
                    }
                }
        }
    }

    if (isEmptyFolder)
    {
    	rmdir(destdir);
    	closedir(dir);
    }
    else 
    {
		closedir(dir);
    }
    
}

void dump_game_executables(char* title_id, char* usb_path)
{
	char dst_path[64];
	char src_path[64];

	sprintf(dst_path, "%s/%s_execs_dump", usb_path, title_id);
	mkdir(dst_path, 0777);

	sprintf(src_path, "/mnt/sandbox/pfsmnt/%s-app0", title_id);

    decrypt_and_dump_execs_in_dir(src_path, dst_path);
}

int _main(struct thread* td) {
  char title_id[64];
  char usb_name[64];
  char usb_path[64];
  int progress;

  initKernel();
  initLibc();
  initPthread();

  uint64_t fw_version = get_fw_version();
  jailbreak(fw_version);

  initSysUtil();

  config.split = 3;
  config.notify = 60;
  config.shutdown = 0;

  nthread_run = 1;
  notify_buf[0] = '\0';
  ScePthread nthread;
  scePthreadCreate(&nthread, NULL, nthread_func, NULL, "nthread");

  printf_notification("PS4 game executables dumper v1.0 initiated!");
  sceKernelSleep(5);

  if (!wait_for_usb(usb_name, usb_path)) {
    sprintf(notify_buf, "Waiting for USB disk...");
    do {
      sceKernelSleep(1);
    } while (!wait_for_usb(usb_name, usb_path));
    notify_buf[0] = '\0';
  }

  if (!wait_for_game(title_id)) {
    sprintf(notify_buf, "Waiting for game to launch...");
    do {
      sceKernelSleep(1);
    } while (!wait_for_game(title_id));
    notify_buf[0] = '\0';
  }

  if (wait_for_bdcopy(title_id) < 100) {
    do {
      sceKernelSleep(1);
      progress = wait_for_bdcopy(title_id);
      sprintf(notify_buf, "Waiting for game to copy\n%u%% completed...", progress);
    } while (progress < 100);
    notify_buf[0] = '\0';
  }

  printf_notification("Decrypting and dumping game executables(sprx/self)\n from %s to %s", title_id, usb_name);
  sceKernelSleep(5);

  dump_game_executables(title_id, usb_path);

  printf_notification("%s selfs/sprxs dumped.\nBye!", title_id);
  sceKernelSleep(10);

  nthread_run = 0;

  return 0;
}
