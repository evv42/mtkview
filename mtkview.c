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

#define APPNAME "mtkview"
#define MAX_FILE_LEN 256

typedef struct{
	unsigned char animated;
	int frames;
	int* delays;
	int disp_frame;
	unsigned char* first_frame;
} mtkview_animation;

typedef struct{
	double zrx;
	double zry;
	char infodisplayed;
	char* title;
	char* info;
	char* viewed;
} mtkview_status;

#define TEXT_DRAWING_POSITION 0,win->ry-(mtk_font_height()*2)

void draw_interface(DWindow* win, Image im, mtkview_status* st, mtkview_animation* anim){
	if(im.width < 0){//Print failures to the screen
		Anchor a = mtk_put_astring(win,TEXT_DRAWING_POSITION,"Can't open image.",WHT);
		mtk_put_astring(win,a.vxanchor,a.vyanchor,(char*)im.data,WHT);
		DFlush(win);
		return;
	}
	int rx = win->rx;
	int ry = win->ry;
	Image final_image = im;
	unsigned char* newdata = NULL;
	int h = 0;
	int v = 0;
	
    if(im.width <= rx && im.height <= ry){//Case A: image is smol
		snprintf(st->info,256,"%s: %dx%d, actual size",st->viewed, im.width, im.height);
		h = (rx-im.width)/2;
		v = (ry-im.height)/2;
	}else{
		if(st->zrx == -1){//Case B: image is big
			snprintf(st->info,256,"%s: %dx%d, resized",st->viewed, im.width, im.height);
			if(!anim->animated)mtk_put_string(win,TEXT_DRAWING_POSITION,"Rendering...",BLK,WHT);
			DFlush(win);
			const double hratio = (double)rx/(double)im.width;
			const double vratio = (double)ry/(double)im.height;
			const double fratio = hratio < vratio ? hratio : vratio;
			int fx = (fratio * (double)im.width);
			int fy = (fratio * (double)im.height);
			h = (rx-fx)/2;
			v = (ry-fy)/2;
			newdata = malloc(fx*fy*4);
			stbir_resize_uint8(im.data, im.width, im.height, 0, newdata, fx, fy, 0, 4);
			final_image = (Image){.data=newdata, .width=fx, .height=fy};
			if(!anim->animated)mtk_put_rectangle(win,0,0,rx,ry,BLK);
		}else{//Case C: image is big and we want details
			snprintf(st->info,256,"%s: %dx%d, actual size (cropped)",st->viewed, im.width, im.height);
			int hr = (double)(im.width - rx) * st->zrx;if(hr<0)hr=0;
			int vr = (double)(im.height - ry) * st->zry;if(vr<0)vr=0;
			final_image = (Image){.data=NULL, .width=im.width-hr, .height=im.height-vr};
			newdata = malloc(final_image.width*final_image.height*4);
			final_image.data = newdata;
			for(int vc=vr;vc<im.height;vc++)\
			for(int hc=hr;hc<im.width;hc++){
				int dest = ( ((vc-vr)*final_image.width)+(hc-hr) )*4;
				int src = ( (vc*im.width)+hc )*4;
				memcpy(newdata+dest,im.data+src,4);
			}
		}
		
	}
	mtk_put_image_buffer(win, h, v, final_image);
	if(newdata != NULL)free(newdata);
	if(st->infodisplayed)mtk_put_string(win,0,win->ry-mtk_font_height(),st->info,0,30,0,WHT);
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
			if(dext != NULL)if(!strcmp(dext,".png") || !strcmp(dext,".jpg") || !strcmp(dext,".jpeg") || !strcmp(dext,".jpe") || !strcmp(dext,".bmp") || !strcmp(dext,".gif") || !strcmp(dext,".qoi") || !strcmp(dext,".PNG") || !strcmp(dext,".JPG") || !strcmp(dext,".JPEG") || !strcmp(dext,".BMP") || !strcmp(dext,".GIF")){
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

Image mtkview_load_image(char* path, mtkview_animation* anim){
	anim->animated = 0;
	char* dext = strrchr(path,'.');
	if(dext != NULL)if(!strcmp(dext,".gif") || !strcmp(dext,".GIF")){//Dirty hack
		FILE* f;
		stbi__context s;
		int x,y,comp;

		if (!(f = stbi__fopen(path, "rb")))
		return (Image){.data=(unsigned char*)"Unable to open file", .width=-1, .height=-1, .handler = ERROR};

		stbi__start_file(&s, f);
		unsigned char* result = 0;

		if (stbi__gif_test(&s))result = stbi__load_gif_main(&s, &anim->delays, &x, &y, &anim->frames, &comp, 4);
		else return (Image){.data=(unsigned char*)"Not a GIF", .width=-1, .height=-1, .handler = ERROR};

		fclose(f);
		if(result == NULL)return (Image){.data=(unsigned char*)"Corrupt GIF", .width=-1, .height=-1, .handler = ERROR};
		if(anim->frames > 1)anim->animated = 1;
		anim->disp_frame = 0;
		anim->first_frame = result;
		return (Image){.data=result, .width=x, .height=y, .handler = STBI_HANDLED};
	}
	return mtk_load_image(path);
}

Image reload_image(Image im, DWindow* win, char* viewed, mtkview_animation* anim){
	if(anim->animated)im.data = anim->first_frame;
	mtk_free_image(im);
	mtk_put_string(win,TEXT_DRAWING_POSITION,"Loading image...",BLK,WHT);
	DFlush(win);
	return mtkview_load_image(viewed, anim);
}

char is_dir(const char *path) {
   struct stat sb;
   if(stat(path, &sb))return 0;
   return S_ISDIR(sb.st_mode);
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
		//for(int i=0; filelist[i] != NULL && index == -1; i++)if(!strcmp(filelist[i],file))index = i;
	}

	mtkview_status* st = malloc(sizeof(mtkview_status));
	mtkview_animation* anim = malloc(sizeof(mtkview_animation));
	if(st == NULL || anim == NULL){
		perror("malloc");
		return 1;
	}
	st->zrx = -1;
	st->zry = -1;
	st->viewed = generate_filepath(NULL, dir, filelist[index]);
    DWindow* win = DInit(640, 480, APPNAME);
	st->title = get_program_status(dir, filelist[index], index+1, direntries);
	st->info = malloc(256);
	st->infodisplayed = 0;
	DChangeName(win, st->title);
	free(st->title);
	Image im = mtkview_load_image(st->viewed, anim);
	
	char confirm_delete = 0;
    while(win->alive){
		GUIRequest grq = DGetRequest(win);
		switch(grq.type){
			case RESIZE_RQ:
				confirm_delete = 0;
				mtk_put_rectangle(win,0,0,win->rx,win->ry,BLK);
				draw_interface(win, im, st, anim);
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
					draw_interface(win, im, st, anim);
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
					if(confirm_delete){
						remove(st->viewed);
						for(int i=0; filelist[i] != NULL; i++)free(filelist[i]);
						free(filelist);
						filelist = get_files(dir, &direntries);
						if(filelist[index] == NULL)index = 0;
						confirm_delete = 0;
					}else{
						mtk_put_string(win,TEXT_DRAWING_POSITION,"Press again to delete",RED,WHT);
						DFlush(win);
						confirm_delete = 1;
						break;
					}
				}else if(grq.data == SK_Escape){//Quit
					DEndProcess(win);
					break;
				}else if(grq.utfkey[0] == '+'){//Toggle 1:1 scale mode
					if(st->zrx == -1){st->zrx = 0;st->zry = 0;}
					else{st->zrx = -1;st->zry = -1;}
					mtk_put_rectangle(win,0,0,win->rx,win->ry,BLK);
					draw_interface(win, im, st, anim);
					break;
				}else if(grq.utfkey[0] == 'i'){//Toggle infomode
					st->infodisplayed = !st->infodisplayed;
					draw_interface(win, im, st, anim);
					break;
				}else{
					break;
				}
			//For non-break'd statements, reload
				st->zrx = -1;st->zry = -1;
				st->viewed = generate_filepath(st->viewed, dir, filelist[index]);
				st->title = get_program_status(dir, filelist[index], index+1, direntries);
				DChangeName(win, st->title);
				free(st->title);
				im = reload_image(im, win, st->viewed, anim);
				confirm_delete = 0;
				mtk_put_rectangle(win,0,0,win->rx,win->ry,BLK);
				draw_interface(win, im, st, anim);
				break;
			default:
				break;
		}
		if(anim->animated){//Accurate enough
			if(anim->delays[0] != 0)usleep((unsigned long)(anim->delays[anim->disp_frame]) * 1000L);
			else usleep(41666);//24fps as default
			if((++anim->disp_frame) > (anim->frames-1))anim->disp_frame = 0;
			im.data = anim->first_frame + ((im.width*im.height*4) * anim->disp_frame);
			draw_interface(win, im, st, anim);
		}else usleep(100000);
    }
    
	if(anim->animated)im.data = anim->first_frame;
	mtk_free_image(im);
	free(st->viewed);
	free(st->info);
	free(st);
	free(anim);
	for(int i=0; filelist[i] != NULL; i++)free(filelist[i]);
	free(filelist);
	return 0;
}
