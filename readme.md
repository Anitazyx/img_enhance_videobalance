### 图像质量增强算法 

---
#### 代码结构
主函数为main.cpp，实现了图像的自适应线性增强与自适应非线性增强（gamma变换）。

```
cd build
cmake ..
make
./test
```
输入图像路径为pic_input，路径下有四个文件夹，内容分别如下  
> src->原图  
src_balance->原图经过了自适应线性增强  
src_balance_gamma -> 自适应线性增强后进行自适应gamma变换  
src_gamma-> 原图经过了自适应gamma变换

观察效果可以认为src_balance_gamma的效果最好。

##### 图像自适应线性增强
通过图像累计分布直方图求出alpha和beta值，并对原图进行如下变换，可起到视觉增强效果 
```math
O(x, y) = alpha * I(x, y) +beta  
```
##### 图像自适应非线性增强
通过均值mean决定整张图片需要达到的亮度均值，这里取的是0.4，因为处理图像大多数偏暗，通过mean求出gamma值，并进行如下变换，可对较暗图像实现暗部增强，亮部减弱效果。
```math
O = (I/255)^g*255
```
以上流程若处理一段视频，使用VideoRead函数，抽取连续视频中的帧进行计算，这里直接取了前30帧，通过算法得到原图的alpha、beta和gamma值，若为视频，则取平均。
##### alpha、beta、gamma值与插件输入的关系
由于本算法均在rgb通道进行处理，而插件读取的格式为NV12需要在YUV通道进行处理，这对于线性变换来讲参数移植是可行的，但对于非线性变换不可以。  
因此若插件**直接在yuv通道处理**（videobalance源码，无需调整），则只能实现自适应线性增强，效果不佳，其转换关系如下，也就是算法输出的contrast、brightness和saturation值：  

```math
contrast = alpha  
```
```math
brightness = (beta + 16*alpha -16)/255  
```
```math
hue = 0  
```
```math
saturation = alpha  
```
但若更改插件算法在RGB通道进行处理，那么无论是线性或非线性增强都可以完成，则直接移植这三个参数即可，其对应关系如下，更改后的videobalance算法在videobalance文件夹下。
```math
contrast = alpha  
```
```math
brightness = beta
```
```math
hue = 0  
```
```math
saturation = gamma
```