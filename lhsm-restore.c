#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <lustre/lustreapi.h>

unsigned long long file_size_tally = 0;
void listdir(const char *name)
{
	DIR *dir;
	struct dirent *entry;
	if (!(dir = opendir(name))) {
		return;
	} else {
        // printf("working on %s\n", name);
    }

	while ((entry = readdir(dir)) != NULL) {
		char path[2048];
        char filebuff[4096];
		struct hsm_user_state hus;
		int rc;
        int released;

		if (*entry->d_name =='/') 
			snprintf(path, sizeof(path), "%s%s", name, entry->d_name);
		else
			snprintf(path, sizeof(path), "%s/%s", name, entry->d_name);
		if (entry->d_type == DT_DIR) {
			if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
				continue;
			listdir(path);
		} else if (entry->d_type == DT_REG) {
			rc = llapi_hsm_state_get(path, &hus);
			if (rc != 0) {
				printf("Error: get hsm state rc %d errno %d for %s\n", rc, errno, path);
				break;
			} else {
                //printf("Got hsm state for %s\n", path);
            }
            released = hus.hus_states & HS_RELEASED;
            if (released) {
                int fd = open(path, O_RDONLY);
                if (fd == 0) {
                    fprintf(stderr,"FailOpen: %d %s\n", errno, path);
                } else {
                    //printf("Recover: %s\n", path);
                    rc = read(fd, &filebuff, sizeof(filebuff));
                    if (rc < 0) { 
                        fprintf(stderr,"Lost: %d %s\n", errno, path); 
                    } else {
                        fprintf(stdout,"Recovered: %s \n", path);
                    }
                close(fd);
                }
            } else {
                fprintf(stdout, "Found: %s\n", path);
            }
		}
	}
	closedir(dir);
}

int main(int argc, char** argv) {
	if (argc >1) {
        char* path=argv[1];
        size_t plen=strlen(path);
        /* trim trailing path delimiter if provided */
        if ((plen > 0) && (path[plen-1] == '/')) {
            plen--;
            path[plen] = 0;
        }
        listdir(path);
    }
	else
	   listdir(".");
	return 0;
}
// vim: tabstop=4 shiftwidth=4 softtabstop=4 smarttab expandtab
