#include "RuntimeGridCanvas.hpp"
#include <iostream>
#include <random>
using namespace kairo::player::detail;
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main()
{
    std::mt19937 rng(55);
    for(unsigned i=0;i<10000;++i)
    {
        const int w=1+int(rng()%4000),h=1+int(rng()%3000),cols=1+int(rng()%256),rows=1+int(rng()%256);
        RuntimeGridCanvas canvas(w,h,cols,rows);
        const auto full=canvas.Rect(0,0,cols,rows);
        Check(full.X>=0&&full.Y>=0&&full.X+full.W<=w&&full.Y+full.H<=h,"Canvas outside viewport");
        if(canvas.Scale())
        {
            const int x=int(rng()%cols),y=int(rng()%rows);const auto cell=canvas.Rect(x,y);
            Check(canvas.Hit(cell.X,cell.Y)==GridPixelRect{x,y,1,1},"Pixel inverse mismatch");
            Check(!canvas.Hit(full.X+full.W,full.Y),"Right edge accepted");
        }
        else Check(!canvas.Hit(0,0),"Zero scale hit");
    }
    std::cout<<"10000 canvas fit and inverse-picking cases passed\n";
}
