

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <iostream>
#include <vector>
#define PI 3.14159265


class crown{
 public:
  int index;
  int raggio;
  int isincrown(double x, double y)
    {
      if (
         ( (x*x + y*y) < ((raggio*index)*(raggio*index)) )
         && ( (x*x + y*y) >= ( (raggio*(index-1))*(raggio*(index-1)) ) ) 
         )
      {
      return 1;
      } else return 0;
          
    }

  crown(int id, int rad)
  {
    index = id;
    raggio = rad;
  }

};

double retta(double x1, double y1, double x2, double y2, double x)
{
  return (x-x1)*(y2-y1)/(x2-x1) + y1;
}

int main ()
{

double x1,y1, x2,y2;
int lato = 10;
std::vector<crown> vcrown;

int runs = 10000;
int crowns = 200;
double crown_occurr[crowns];

for (int j=1; j<=crowns; j++)
{
  vcrown.push_back(crown(j,lato));
  crown_occurr[j-1] = 0;
}


for (int r=0; r<runs; r++)
{
//  if (!(r % 1000))
//      std:: cout << "run " << r << std::endl;
  double radius = rand() % lato*crowns;
  double angle = 2*PI*((double)(rand() % 100)/100);
  x1 = radius*cos(angle);
  y1 = radius*sin(angle);

  do{
  radius = rand() % lato*crowns;
  angle = 2*PI*((double)(rand() % 100)/100);
  x2 = radius*cos(angle);
  }
  while(x1>x2);

  y2 = radius*sin(angle);

//  std::cout << x1 << " " << y1 << " " << x2 << " " << y2 << std::endl;
  for (std::vector<crown>::iterator ii=vcrown.begin();
      ii != vcrown.end(); ii++)
  {
    bool found = 0;
    for (int i=x1; i<= x2 && found == 0; i++ )
    {
      if (ii->isincrown(i, retta(x1,y1,x2,y2,i)) )
      {
//        std::cout << "la retta passa per la corona " << ii->index << std::endl;
//        std:: cout << i << " " << retta(x1,y1,x2,y2,i) << std::endl;
        found = 1;
        crown_occurr[ii->index - 1 ]++;
      }
    }
  }
}
for (int k = 0; k<crowns ; k++)
  {
    double area = PI*( lato*(k+1)*lato*(k+1) - lato*(k)*lato*(k) );
    double freq = (crown_occurr[k]/runs) /area;
    std::cout << "crown " << k << " occurences: " << crown_occurr[k] << " freq " << freq  << std:: endl; 
  }
}

