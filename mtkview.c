/* See LICENSE file for copyright and license details. */
#include <stdio.h>
#include <unistd.h>
#define STBI_ASSERT(x)//Some GIFs will fail asserts otherwhise
#define DMTK_IMPLEMENTATION
#include "dmtk.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"
#include <dirent.h> 
#include <limits.h>
#include <stdlib.h>

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
	char* title;
	char* viewed;
} mtkview_status;

void draw_interface(DWindow* win, Image im, mtkview_status* st, mtkview_animation* anim){
	if(im.width < 0){//Print failtures to the screen
		Anchor a = mtk_put_astring(win,10,10,"Can't open image.",WHT);
		mtk_put_astring(win,a.vxanchor,a.vyanchor,(char*)im.data,WHT);
		DFlush(win);
		return;
	}
	int rx = win->rx;
	int ry = win->ry;
	
    if(im.width <= rx && im.height <= ry){//Case A: image is smol
		int h = (rx-im.width)/2;
		int v = (ry-im.height)/2;
		mtk_put_image_buffer(win, h, v, im);
	}else{
		if(st->zrx == -1){//Case B: image is big
			if(!anim->animated)mtk_put_string(win,10,10,"Rendering...",BLK,WHT);
			DFlush(win);
			const double hratio = (double)rx/(double)im.width;
			const double vratio = (double)ry/(double)im.height;
			const double fratio = hratio < vratio ? hratio : vratio;
			int fx = (fratio * (double)im.width);
			int fy = (fratio * (double)im.height);
			int h = (rx-fx)/2;
			int v = (ry-fy)/2;
			unsigned char* newdata = malloc(fx*fy*4);
			stbir_resize_uint8(im.data, im.width, im.height, 0, newdata, fx, fy, 0, 4);
			Image new = (Image){.data=newdata, .width=fx, .height=fy};
			if(!anim->animated)mtk_put_rectangle(win,0,0,rx,ry,BLK);
			mtk_put_image_buffer(win, h, v, new);
			free(newdata);
		}else{//Case C: image is big and we want details
			const int hr = (double)im.width * st->zrx;
			const int vr = (double)im.height * st->zry;
			Image new = (Image){.data=NULL, .width=im.width-hr, .height=im.height-vr};
			unsigned char* newdata = malloc(new.width*new.height*4);
			new.data = newdata;
			for(int v=vr;v<im.height;v++)\
			for(int h=hr;h<im.width;h++){
				int dest = ( ((v-vr)*new.width)+(h-hr) )*4;
				int src = ( (v*im.width)+h )*4;
				newdata[dest] = im.data[src];dest++;src++;
				newdata[dest] = im.data[src];dest++;src++;
				newdata[dest] = im.data[src];dest++;src++;
				newdata[dest] = im.data[src];
			}
			if(!anim->animated)mtk_put_rectangle(win,0,0,rx,ry,BLK);
			mtk_put_image_buffer(win, 0, 0, new);
			free(newdata);
		}
		
	}
	DFlush(win);
}

//Generates a NULL-terminated list of images in the folder.
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

Image mtkview_load_image(char* path, mtkview_animation* anim){
	anim->animated = 0;
	char* dext = strrchr(path,'.');
	if(dext != NULL)if(!strcmp(dext,".gif") || !strcmp(dext,".GIF")){//Dirty hack
		FILE* f;
		stbi__context s;
		int x;
		int y;

		if (!(f = stbi__fopen(path, "rb")))
		return (Image){.data=(unsigned char*)"Unable to open file", .width=-1, .height=-1, .handler = ERROR};

		stbi__start_file(&s, f);
		int comp;
		unsigned char *result = 0;

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
	mtk_put_string(win,10,10,"Loading image...",BLK,WHT);
	DFlush(win);
	return mtkview_load_image(viewed, anim);
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
	char* file = strrchr(dir,'/');
	*file = 0;
	file++;
	
	//Get list of images
	int direntries = 0;
	char** filelist = get_files(dir, &direntries);
	qsort(filelist, direntries, sizeof(char*), (int (*)(const void *, const void *))qsortstrcmp);
	
	int index=-1;
	//Retroactively get the index of the current file
	char** iptr = bsearch(&file, filelist, direntries, sizeof(char*), (int (*)(const void *, const void *))qsortstrcmp);
	printf("%d\n",(iptr-filelist));
	index = iptr-filelist;
	//for(int i=0; filelist[i] != NULL && index == -1; i++)if(!strcmp(filelist[i],file))index = i;

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
			case KEYB_RQ:
				if(grq.data == SK_Right || grq.data == SK_Down){
					index++;
					if(index > (direntries-1))index = 0;
				}else if(grq.data == SK_Left || grq.data == SK_Up){
					index--;
					if(index < 0)index=direntries-1;
				}else if(grq.data == SK_Page_Up){
					index-=10;
					if(index < 0)index=direntries-1;
				}else if(grq.data == SK_Page_Down){
					index+=10;
					if(index > (direntries-1))index = 0;
				}else if(grq.data == SK_Home || grq.data == SK_KP_Home){
					index = 0;
				}else if(grq.data == SK_End || grq.data == SK_KP_End){
					index=direntries-1;
				}else if(grq.data == SK_Delete){
					if(confirm_delete){
						remove(st->viewed);
						for(int i=0; filelist[i] != NULL; i++)free(filelist[i]);
						free(filelist);
						filelist = get_files(dir, &direntries);
						qsort(filelist, direntries, sizeof(char*), (int (*)(const void *, const void *))qsortstrcmp);
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
				}else if(grq.utfkey[0] == '+'){
					if(st->zrx == -1){st->zrx = 0;st->zry = 0;}
					else{st->zrx = -1;st->zry = -1;}
					mtk_put_rectangle(win,0,0,win->rx,win->ry,BLK);
					draw_interface(win, im, st, anim);
					break;
				}else{
					break;
				}
				goto reload_title;
			case MOUSE_RQ:
				if(grq.data == 3){
					index++;
					if(filelist[index] == NULL)index = 0;
				}else if(grq.data == 2){
					index--;
					if(index < 0)index=direntries-1;
				}else{
					if(st->zrx < 0)break;
					st->zrx = grq.x / (double)win->rx;
					st->zry = grq.y / (double)win->ry;
					mtk_put_rectangle(win,0,0,win->rx,win->ry,BLK);
					draw_interface(win, im, st, anim);
					break;
				}
			reload_title:
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
	free(st);
	free(anim);
	for(int i=0; filelist[i] != NULL; i++)free(filelist[i]);
	free(filelist);
	return 0;
}
