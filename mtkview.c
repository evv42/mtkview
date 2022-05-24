/* See LICENSE file for copyright and license details. */
#include <stdio.h>
#include <unistd.h>
#include "dmtkgui.h"
#include "dmtk.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"
#include <dirent.h> 
#include <limits.h>
#include <stdlib.h>

#define APPNAME "mtkview"
#define MAX_FILE_LEN 256

void draw_interface(DWindow* win, Image im){
	if(im.width < 0){
		mtk_put_rectangle(win,0,0,win->rx,win->ry,BLK);
		Anchor a = mtk_put_astring(win,10,10,"Can't open image.",WHT);
		mtk_put_astring(win,a.vxanchor,a.vyanchor,im.data,WHT);
		DFlush(win);
		return;
	}
	int rx = win->rx;
	int ry = win->ry;
	
	mtk_put_rectangle(win,0,0,win->rx,win->ry,BLK);
	
    if(im.width <= rx && im.height <= ry){
		int h = (rx-im.width)/2;
		int v = (ry-im.height)/2;
		mtk_put_image_buffer(win, h, v, im);
	}else{
		mtk_put_astring(win,10,10,"Rendering...",WHT);
		DFlush(win);
		const double hratio = (double)rx/(double)im.width;
		const double vratio = (double)ry/(double)im.height;
		const double fratio = hratio < vratio ? hratio : vratio;
		int fx = (fratio * (double)im.width);
		int fy = (fratio * (double)im.height);
		int h = (rx-fx)/2;
		int v = (ry-fy)/2;
		char* newdata = malloc(fx*fy*4);
		stbir_resize_uint8((unsigned char*)im.data, im.width, im.height, 0, (unsigned char*)newdata, fx, fy, 0, 4);
		Image new = (Image){.data=newdata, .width=fx, .height=fy};
		mtk_put_rectangle(win,0,0,rx,ry,BLK);
		mtk_put_image_buffer(win, h, v, new);
		free(newdata);
		
	}
	DFlush(win);
}

//Gives a worst-case number of files in dir
int count_dir_entries(char* directory){
	int i = 0;
	DIR* d;
	struct dirent* dir;
	d = opendir(directory);
	if (d) {
		while ((dir = readdir(d)) != NULL)i++;
		closedir(d);
	}else{perror("opendir");}
	return i+1;
}

//Generates a NULL-terminated list of images in the folder, and properly recount files.
char** get_files(char* orgfile, int* entries){
	char** filelist = malloc(sizeof(char*)*(*entries + 1));
	DIR* d;
	struct dirent* dir;
	d = opendir(orgfile);
	*entries = 0;
	if (d) {
		while ((dir = readdir(d)) != NULL){
			char* dext = strrchr(dir->d_name,'.');
			if(dext != NULL)if(!strcmp(dext,".png") || !strcmp(dext,".jpg") || !strcmp(dext,".jpeg") || !strcmp(dext,".bmp") || !strcmp(dext,".gif") || !strcmp(dext,".qoi") || !strcmp(dext,".PNG") || !strcmp(dext,".JPG") || !strcmp(dext,".JPEG") || !strcmp(dext,".BMP") || !strcmp(dext,".GIF") || !strcmp(dext,".QOI")){
				filelist[*entries] = malloc(MAX_FILE_LEN);
				strcpy(filelist[*entries],dir->d_name);
				*entries += 1;
			}
		}
		closedir(d);
	}else{perror("opendir");}
	filelist[*entries] = NULL;
	
	return filelist;
}

//Generates a string to be displayed on the status bar.
char* get_program_status(char* dir, char* file, int index, int total){
	char* stat = malloc(strlen(APPNAME) + strlen(dir) + strlen(file) + 64);
	*stat=0;
	sprintf(stat,"%s - %s - %s (%d/%d)", APPNAME, file, strrchr(dir,'/')+1, index, total);
	return stat;
}

int qsortstrcmp(char** a, char** b){
	return strcoll(*a,*b);
}

char* generate_filepath(char* old, char* dir, char* file){
	if(old != NULL)free(old);
	char* new = malloc(strlen(dir)+strlen(file)+1+1);
	*new = 0;
	strcat(new,dir);
	strcat(new,"/");
	strcat(new,file);
	return new;
}

Image reload_image(Image im, DWindow* win, char* viewed){
	mtk_free_image(im);
	mtk_put_rectangle(win,0,0,win->rx,win->ry,BLK);
	mtk_put_astring(win,10,10,"Loading image...",WHT);
	DFlush(win);
	return mtk_load_image(viewed);
}

int main(int argc, char** argv){
	if(argc < 2)return 1;

	//Get directory
	char* dir = realpath(argv[1], NULL);
	if(dir == NULL){
		perror("realpath");
		return 1;
	}
	char* file = strrchr(dir,'/');
	*file = 0;
	file++;
	
	//Get list of images
	int direntries = count_dir_entries(dir);
	char** filelist = get_files(dir, &direntries);
	qsort(filelist, direntries, sizeof(char*), (__compar_fn_t)qsortstrcmp);
	
	int index=-1;
	//Retroactively get the index of the current file
	for(int i=0; filelist[i] != NULL && index == -1; i++)if(!strcmp(filelist[i],file))index = i;
		
	char* viewed = generate_filepath(NULL, dir, filelist[index]);
	
    DWindow* win = DInit(640, 480, APPNAME);
	char* status = get_program_status(dir, filelist[index], index+1, direntries);
	DChangeName(win, status);
	free(status);
	Image im = mtk_load_image(viewed);
	
	char confirm_delete = 0;
    while(win->alive){
		GUIRequest grq = DGetRequest(win);
		switch(grq.type){
			case RESIZE_RQ:
				confirm_delete = 0;
				draw_interface(win, im);
				break;
			case KEYB_RQ:
				if(grq.data == SK_Right || grq.data == SK_Down){
					index++;
					if(filelist[index] == NULL)index = 0;
				}else if(grq.data == SK_Left || grq.data == SK_Up){
					index--;
					if(index < 0)index=direntries-1;
				}else if(grq.data == SK_Home || grq.data == SK_KP_Home){
					index = 0;
				}else if(grq.data == SK_End || grq.data == SK_KP_End){
					index=direntries-1;
				}else if(grq.data == SK_Delete){
					if(confirm_delete){
						remove(viewed);
						free(filelist);
						filelist = get_files(dir, &direntries);
						qsort(filelist, direntries, sizeof(char*), (__compar_fn_t)qsortstrcmp);
						if(filelist[index] == NULL)index = 0;
						confirm_delete = 0;
					}else{
						mtk_put_string(win,10,10,"Press again to delete",RED,WHT);
						DFlush(win);
						confirm_delete = 1;
						break;
					}
				}else if(grq.data == SK_Escape){
					DEndProcess(win);
					break;
				}else{
					break;
				}
				viewed = generate_filepath(viewed, dir, filelist[index]);
				status = get_program_status(dir, filelist[index], index+1, direntries);
				DChangeName(win, status);
				free(status);
				im = reload_image(im, win, viewed);
				confirm_delete = 0;
				draw_interface(win, im);
				break;
			case MOUSE_RQ:
				if(grq.data == 3){
					index++;
					if(filelist[index] == NULL)index = 0;
				}else if(grq.data == 2){
					index--;
					if(index < 0)index=direntries-1;
				}else{
					break;
				}
				viewed = generate_filepath(viewed, dir, filelist[index]);
				status = get_program_status(dir, filelist[index], index+1, direntries);
				DChangeName(win, status);
				free(status);
				im = reload_image(im, win, viewed);
				confirm_delete = 0;
				draw_interface(win, im);
				break;
			default:
				break;
		}
		usleep(100000);
    }

    free(filelist);
    free(dir);
    return 0;
}
