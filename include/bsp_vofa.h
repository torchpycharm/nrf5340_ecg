#ifndef VOFA_H
#define VOFA_H


#ifdef __cplusplus
extern "C" {
#endif
#define NOFCHANEL 10
	
typedef union
{
    float fdata[NOFCHANEL];
    unsigned char cdata[NOFCHANEL*4+4];
}transform;	
int TO_Vofa_DATA(void);

#ifdef __cplusplus
}
#endif
#endif

