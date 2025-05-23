/* See LICENSE file for copyright and license details. */
#include <stdio.h>
#include <unistd.h>
#define STBI_ASSERT(x)//Some GIFs will fail asserts otherwhise
#define DMTK_IMPLEMENTATION
#include "dmtk/dmtk.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"
#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>

#define APPNAME "mtkview"
#define MAX_FILE_LEN 256

typedef struct{
	char* viewed;
	
	double zrx;
	double zry;
	
	//char* title;
	char confirm_delete;
	
	unsigned char infodisplayed;
	char* info;
	
	Image im;
	
	unsigned char animated;
	unsigned char run;
	int frames;
	int* delays;
	int disp_frame;
	unsigned char* first_frame;
} mtkview_status;

#define TEXT_DRAWING_POSITION 4,win->ry-(mtk_font_height()*2)-4

void draw_interface(DWindow* win, mtkview_status* st){
	Anchor a;
	if(st->im.width < 0){//Print failures to the screen
		a = mtk_put_astring(win,TEXT_DRAWING_POSITION,"Can't open image.",WHT);
		mtk_put_astring(win,a.vxanchor,a.vyanchor,(char*)st->im.data,WHT);
		DFlush(win);
		return;
	}
	const int rx = win->rx;
	const int ry = win->ry;
	Image final_image = st->im;
	unsigned char* newdata = NULL;
	int h = 0;
	int v = 0;

	snprintf(st->info,256,"%s: %dx%d",st->viewed, st->im.width, st->im.height);
    if(st->im.width <= rx && st->im.height <= ry){//Case A: image is smol
		strncat(st->info,", actual size", 256);
		h = (rx-st->im.width)/2;
		v = (ry-st->im.height)/2;
	}else{
		if(st->zrx == -1){//Case B: image is big
			strncat(st->info,", resized", 256);
			if(!st->animated)mtk_put_string(win,TEXT_DRAWING_POSITION,"Rendering...",BLK,WHT);
			DFlush(win);
			const double hratio = (double)rx/(double)st->im.width;
			const double vratio = (double)ry/(double)st->im.height;
			const double fratio = hratio < vratio ? hratio : vratio;
			int fx = (fratio * (double)st->im.width);
			int fy = (fratio * (double)st->im.height);
			h = (rx-fx)/2;
			v = (ry-fy)/2;
			newdata = malloc(fx*fy*4);
			stbir_resize_uint8(st->im.data, st->im.width, st->im.height, 0, newdata, fx, fy, 0, 4);
			/*version 2, not used yet
			stbir_resize_uint8_linear(st->im.data, st->im.width, st->im.height, 0, newdata, fx, fy, 0, (stbir_pixel_layout) 4);*/
			final_image = (Image){.data=newdata, .width=fx, .height=fy};
			if(!st->animated)mtk_put_rectangle(win,0,0,rx,ry,BLK);
		}else{//Case C: image is big and we want details
			strncat(st->info,", actual size (cropped)", 256);
			int hr = (double)(st->im.width - rx) * st->zrx;if(hr<0)hr=0;
			int vr = (double)(st->im.height - ry) * st->zry;if(vr<0)vr=0;
			final_image = (Image){.data=NULL, .width=st->im.width-hr, .height=st->im.height-vr};
			newdata = malloc(final_image.width*final_image.height*4);
			final_image.data = newdata;
			for(int vc=vr;vc<st->im.height;vc++)\
			for(int hc=hr;hc<st->im.width;hc++){
				int dest = ( ((vc-vr)*final_image.width)+(hc-hr) )*4;
				int src = ( (vc*st->im.width)+hc )*4;
				memcpy(newdata+dest,st->im.data+src,4);
			}
		}

	}
	mtk_put_image_buffer(win, h, v, final_image);
	if(newdata != NULL)free(newdata);
	if(st->infodisplayed){
		a = mtk_put_string(win,TEXT_DRAWING_POSITION,st->info,0,30,0,WHT);
		if(st-> animated){
			snprintf(st->info,256,"animated: frame %4d/%4d, delay %4dms",st->disp_frame+1, st->frames, st->delays[st->disp_frame]);
			if(!st->run)strncat(st->info," (frame-by-frame mode)", 256);
			mtk_put_string(win, a.vxanchor, a.vyanchor,st->info,0,30,0,WHT);
		}
	}
	DFlush(win);
}

int qsortstrcmp(char** a, char** b){
	return strcoll(*a,*b);
}

//Generates a NULL-terminated sorted list of images in the folder.
char** get_files(char* orgfile, int* entries){
	unsigned int list_size = 10;
	char** filelist = malloc(sizeof(char*)* list_size);
	struct dirent* dir;
	DIR* d = opendir(orgfile);
	*entries = 0;
	if (d) {
		while ((dir = readdir(d)) != NULL){
			char* dext = strrchr(dir->d_name,'.');
			if(dext != NULL)if(!strcasecmp(dext,".png") || !strcasecmp(dext,".jpg") || !strcasecmp(dext,".jpeg") || !strcasecmp(dext,".jpe") || !strcasecmp(dext,".bmp") || !strcasecmp(dext,".gif") || !strcasecmp(dext,".qoi") ){
				if(*entries > (list_size-2))filelist = realloc(filelist,sizeof(char*)* (++list_size));
				filelist[*entries] = malloc(MAX_FILE_LEN);
				strcpy(filelist[*entries],dir->d_name);
				*entries += 1;
			}
		}
		closedir(d);
	}else{perror("opendir");}
	filelist[*entries] = NULL;
	qsort(filelist, *entries, sizeof(char*), (int (*)(const void *, const void *))qsortstrcmp);
	
	return filelist;
}

//Generates a string to be displayed on the status bar.
char* get_program_status(char* dir, char* file, int index, int total){
	char* stat = malloc(256);
	snprintf(stat,256,"%s - %s - %s (%d/%d)", APPNAME, file, strrchr(dir,'/')+1, index, total);
	return stat;
}

char* generate_filepath(char* old, char* dir, char* file){
	if(old != NULL)free(old);
	unsigned int size = strlen(dir)+strlen(file)+1+1;
	char* new = malloc(size);
	snprintf(new,size,"%s/%s",dir,file);
	return new;
}

void mtkview_load_image(mtkview_status* st){
	st->animated = 0;
	char* dext = strrchr(st->viewed,'.');
	if(dext != NULL)if(!strcmp(dext,".gif") || !strcmp(dext,".GIF")){//Dirty hack
		FILE* f;
		stbi__context s;
		int x,y,comp;

		if (!(f = stbi__fopen(st->viewed, "rb"))){
			st->im = (Image){.data=(unsigned char*)"Unable to open file", .width=-1, .height=-1, .handler = ERROR};
			return;
		}

		stbi__start_file(&s, f);
		unsigned char* result = 0;

		if (stbi__gif_test(&s))result = stbi__load_gif_main(&s, &st->delays, &x, &y, &st->frames, &comp, 4);
		else{
			st->im = (Image){.data=(unsigned char*)"Not a GIF", .width=-1, .height=-1, .handler = ERROR};
			return;
		}

		fclose(f);
		if(result == NULL){
			st->im = (Image){.data=(unsigned char*)"Corrupt GIF", .width=-1, .height=-1, .handler = ERROR};
			return;
		}
		if(st->frames > 1)st->animated = 1;
		st->disp_frame = 0;
		st->first_frame = result;
		st->im = (Image){.data=result, .width=x, .height=y, .handler = STBI_HANDLED};
		return;
	}
	st->im = mtk_load_image(st->viewed);
}

void reload_image(DWindow* win, mtkview_status* st){
	//st->zrx = -1;st->zry = -1;
	if(st->animated)st->im.data = st->first_frame;
	mtk_free_image(st->im);
	mtk_put_string(win,TEXT_DRAWING_POSITION,"Loading image...",BLK,WHT);
	DFlush(win);
	mtkview_load_image(st);
}

char is_dir(const char *path) {
   struct stat sb;
   if(stat(path, &sb))return 0;
   return S_ISDIR(sb.st_mode);
}

void set_displayed_image(mtkview_status* st, char* file, char* dir){
	st->zrx = -1;
	st->zry = -1;
	st->confirm_delete = 0;
	st->viewed = generate_filepath(st->viewed, dir, file);
}

int main(int argc, char** argv){
	if(argc < 2)return 1;

	//Get directory
	char dbuf[PATH_MAX];
	char* dir = realpath(argv[1], dbuf);
	if(dir == NULL){
		perror("realpath");
		return 1;
	}
	char* file = NULL;
	if(!is_dir(dir)){
		file = strrchr(dir,'/');
		*file = 0;
		file++;
	}

	//Get list of images
	int direntries = 0;
	char** filelist = get_files(dir, &direntries);

	char* title;
	int index;
	if(file == NULL){
		file = filelist[0];
		index = 0;
	}else{
		index=-1;
		//Retroactively get the index of the current file
		char** iptr = bsearch(&file, filelist, direntries, sizeof(char*), (int (*)(const void *, const void *))qsortstrcmp);
		if(iptr == NULL)return 2;
		index = iptr-filelist;
	}

	mtkview_status* st = malloc(sizeof(mtkview_status));
	if(st == NULL){
		perror("malloc");
		return 1;
	}
	//st->zrx = -1;
	//st->zry = -1;
	//st->viewed = generate_filepath(NULL, dir, filelist[index]);
	st->info = malloc(256);
	st->infodisplayed = 0;
	st->run = 1;
	st->viewed = NULL;
	set_displayed_image(st, filelist[index], dir);
	
	DWindow* win = DInit(640, 480, APPNAME);
	
	title = get_program_status(dir, filelist[index], index+1, direntries);
	DChangeName(win, title);
	free(title);
	
	mtkview_load_image(st);

    while(win->alive){
		
		struct timeval start, end;
		gettimeofday(&start, NULL);
		if(st->animated && st->run)draw_interface(win, st);
		
		GUIRequest grq = DGetRequest(win);
		switch(grq.type){
			case RESIZE_RQ:
				st->confirm_delete = 0;
				mtk_put_rectangle(win,0,0,win->rx,win->ry,BLK);
				draw_interface(win, st);
				break;
			case KEYB_RQ: case MOUSE_RQ:
				if(grq.data == SK_Right || grq.data == SK_Down || grq.data == 3){//Go to next image
					index++;
					if(index > (direntries-1))index = 0;
				}else if(grq.data == SK_Left || grq.data == SK_Up || grq.data == 2){//Go to prev image
					index--;
					if(index < 0)index=direntries-1;
				}else if(grq.data == 1){//Move view area in 1:1 scale mode
					if(st->zrx < 0)break;
					st->zrx = grq.x / (double)win->rx;
					st->zry = grq.y / (double)win->ry;
					mtk_put_rectangle(win,0,0,win->rx,win->ry,BLK);
					draw_interface(win, st);
					break;
				}else if(grq.data == SK_Page_Up){//Go back 10 images
					index-=10;
					if(index < 0)index=direntries-1;
				}else if(grq.data == SK_Page_Down){//Skip 10 images
					index+=10;
					if(index > (direntries-1))index = 0;
				}else if(grq.data == SK_Home || grq.data == SK_KP_Home){//First image
					index = 0;
				}else if(grq.data == SK_End || grq.data == SK_KP_End){//Last image (not that useful)
					index=direntries-1;
				}else if(grq.data == SK_Delete){//Delete current image (with confirm)
					if(st->confirm_delete){
						remove(st->viewed);
						for(int i=0; filelist[i] != NULL; i++)free(filelist[i]);
						free(filelist);
						filelist = get_files(dir, &direntries);
						if(filelist[index] == NULL)index = 0;
						st->confirm_delete = 0;
					}else{
						mtk_put_string(win,TEXT_DRAWING_POSITION,"Press again to delete",RED,WHT);
						DFlush(win);
						st->confirm_delete = 1;
						break;
					}
				}else if(grq.data == SK_Escape){//Quit
					DEndProcess(win);
					break;
				}else if(grq.utfkey[0] == '+'){//Toggle 1:1 scale mode
					if(st->zrx == -1){st->zrx = 0;st->zry = 0;}
					else{st->zrx = -1;st->zry = -1;}
					mtk_put_rectangle(win,0,0,win->rx,win->ry,BLK);
					draw_interface(win, st);
					break;
				}else if(grq.utfkey[0] == 'i'){//Toggle infomode
					st->infodisplayed = !st->infodisplayed;
					if(st->animated){/*reload gif from first frame*/
						mtk_put_rectangle(win,0,0,win->rx,win->ry,BLK);
						st->disp_frame = 0;
						st->im.data = st->first_frame + ((st->im.width*st->im.height*4) * st->disp_frame);
					}
					draw_interface(win, st);
					break;
				}else if(grq.utfkey[0] == 's'){
					st->run = !st->run;
					if(st->animated && st->run){/*reload gif from first frame if disabled*/
						mtk_put_rectangle(win,0,0,win->rx,win->ry,BLK);
						st->disp_frame = 0;
						st->im.data = st->first_frame + ((st->im.width*st->im.height*4) * st->disp_frame);
					}
					draw_interface(win, st);
					break;
				}else if(grq.utfkey[0] == 'b' && st->animated && !st->run){
					st->disp_frame--;
					if(st->disp_frame < 0)st->disp_frame = st->frames-1;
					st->im.data = st->first_frame + ((st->im.width*st->im.height*4) * st->disp_frame);
					draw_interface(win, st);
					break;
				}else if(grq.utfkey[0] == 'n' && st->animated && !st->run){
					st->disp_frame++;
					if(st->disp_frame >= st->frames)st->disp_frame = 0;
					st->im.data = st->first_frame + ((st->im.width*st->im.height*4) * st->disp_frame);
					draw_interface(win, st);
					break;
				}else{
					break;
				}
			/*For non-break'd statements, reload*/
				set_displayed_image(st, filelist[index], dir);
				
				title = get_program_status(dir, filelist[index], index+1, direntries);
				DChangeName(win, title);
				free(title);
				
				reload_image(win, st);
				mtk_put_rectangle(win,0,0,win->rx,win->ry,BLK);
				draw_interface(win, st);
				break;
			default:
				break;
		}
		if(st->animated && st->run){
			/*Determine how long we took to arrive here*/
			uint64_t then = (start.tv_sec * 1000000) + start.tv_usec;
			uint64_t delay = (unsigned long)(st->delays[st->disp_frame]) * 1000L;
			gettimeofday(&end, NULL);
			uint64_t now = (end.tv_sec * 1000000) + end.tv_usec;
			uint32_t total_usec = now-then;
			
			/*sleep for the programmed delay minus the process time if any and possible*/
			if(st->delays[st->disp_frame] != 0 && total_usec < delay)usleep(delay - total_usec);
			else usleep(41666);//24fps as default
			
			/*reset frame counter if we got past the last*/
			if((++st->disp_frame) > (st->frames-1))st->disp_frame = 0;
			
			/*change data offset*/
			st->im.data = st->first_frame + ((st->im.width*st->im.height*4) * st->disp_frame);
		}else usleep(100000);
    }
    
	if(st->animated)st->im.data = st->first_frame;
	mtk_free_image(st->im);
	free(st->viewed);
	free(st->info);
	free(st);
	for(int i=0; filelist[i] != NULL; i++)free(filelist[i]);
	free(filelist);
	
	return 0;
}
