/* random password generator */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "world.h"
#include "externs.h"

char salt[21];

void initrand(){
	time_t now;
	long x;
	time(&now);
	x=now;
	srandom(x);
}

void initauth(struct client *ccl){
	struct wpacket spk;
	int x, len=sizeof(struct wpacket);
	spk.type=WP_AUTH;
	rpgen(spk.d.auth.pass);
	x=send(ccl->fd, &spk, len, 0);
}


char *rpgen(char *dest){
	int i=0;
	long x;
	for(i=0; i<20; i++){
		x=random();
		salt[i]=58 + (x & 0x3f);
	}
	salt[20]='\0';
	if(dest!=(char*)NULL){
		strcpy(dest, salt);
		return(dest);
	}
	return(salt);
}

/* return server number, or -1 on failure */
short pwcheck(char *cpasswd, long val){
	int i;
	for(i=0; i<snum; i++){
		if(val==chk(slist[i].pass, cpasswd)){
			return(i+1);
		}
	}
	fprintf(stderr, "server failed authentication\n");
	return(-1);
}

/* unified, hopefully unique password check function */
long chk(char *s1, char *s2){
	int i, j=0;
	int m1, m2;
	static long rval[2]={0, 0};
	rval[0]=0L;
	rval[1]=0L;
	m1=strlen(s1);
	m2=strlen(s2);
	for(i=0; i<m1; i++){
		rval[0]+=s1[i];
		rval[0]<<=5;
	}
	for(j=0; j<m2; j++){
		rval[1]+=s2[j];
		rval[1]<<=3;
	}
	for(i=0; i<m1; i++){
		rval[1]+=s1[i];
		rval[1]<<=(3+rval[0]%5);
		rval[0]+=s2[j];
		rval[0]<<=(3+rval[1]%3);
		j=rval[0]%m2;
	}
	return(rval[0]+rval[1]);
}
