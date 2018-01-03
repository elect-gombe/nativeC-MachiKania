#include <sys.h>
int i;

int recursive(int arg){
  if(arg==0){
    return 1;
  }
  return arg*recursive(arg-1);
}

int main(int argc,char **argv){
  int i;
  for(i=1;i<10;i++){
    setcursorcolor(i);
    printf(" %d!=%d\n",i,recursive(i));
  }
  return 0;
}
