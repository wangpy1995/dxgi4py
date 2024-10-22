import ctypes
from ctypes import *

import cv2
import numpy as np
import win32gui

# 加载dll库
dxgi = ctypes.CDLL("d:/dxgi4py.dll")
dxgi.grab.argtypes = (POINTER(ctypes.c_ubyte), ctypes.c_int, c_int, c_int, c_int)
dxgi.grab.restype = POINTER(c_ubyte)

# 获取窗口hwnd
windowTitle = 'OBS 30.2.2 - Profile: game - Scenes: Untitled'
# windowTitle = '尘白禁区'
hwnd = win32gui.FindWindow(None, windowTitle)
windll.user32.SetProcessDPIAware()
# time.sleep(10)
# 初始化
dxgi.init_dxgi(hwnd)

# 指定截图区域(这里示例为截取整个窗口)
left, top, right, bottom = win32gui.GetWindowRect(hwnd)
shotLeft, shotTop = 0, 0
height = bottom - top
width = right - left
# 创建numpy array用于接收截图结果
shot = np.ndarray((height, width, 4), dtype=np.uint8)
shotPointer = shot.ctypes.data_as(POINTER(c_ubyte))
# 截图
for i in range(0, 15):
    buffer = dxgi.grab(shotPointer, shotLeft, shotTop, width, height)
    # 获取结果，跳过内存拷贝
    image = np.ctypeslib.as_array(buffer, shape=(1080, 1920, 4))
    # 保存图像到文件
    cv2.imwrite('test_dxgi/sample_pic'+str(i)+'.png', image)

# 转为BGR形式
img = cv2.cvtColor(image, cv2.COLOR_BGRA2BGR)
cv2.imshow('sample_pic', img)
cv2.waitKey(0)
# 不再使用时销毁
dxgi.destroy()
