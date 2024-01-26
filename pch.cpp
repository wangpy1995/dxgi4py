import ctypes
from ctypes import *

import cv2
import numpy as np
import win32gui

# ����dll��
dxgi = ctypes.CDLL("d:/dxgi4py.dll")
dxgi.grab.argtypes = (POINTER(ctypes.c_ubyte), ctypes.c_int, c_int, c_int, c_int)
dxgi.grab.restype = POINTER(c_ubyte)

# ��ȡ����hwnd
windowTitle = 'BlueStacks'
hwnd = win32gui.FindWindow(None, windowTitle)
windll.user32.SetProcessDPIAware()

# ��ʼ��
dxgi.init_dxgi(hwnd)

# ָ����ͼ����(����ʾ��Ϊ��ȡ��������)
left, top, right, bottom = win32gui.GetWindowRect(hwnd)
shotLeft, shotTop = 0, 0
height = bottom - top
width = right - left
shotRight, shotBottom = shotLeft + width, shotTop + height
# ����numpy array���ڽ��ս�ͼ���
shot = np.ndarray((height, width, 4), dtype = np.uint8)
shotPointer = shot.ctypes.data_as(POINTER(c_ubyte))
# ��ͼ
buffer = dxgi.grab(shotPointer, shotLeft, shotTop, shotRight, shotBottom)
# ��ȡ���
image = np.fromiter(buffer, dtype = np.uint8, count = height * width * 4).reshape((height, width, 4))
# תΪBGR��ʽ
img = cv2.cvtColor(image, cv2.COLOR_BGRA2BGR)
cv2.imshow('sample_pic', img)
cv2.waitKey(0)
cv2.destroyAllWindows()
# ����ʹ��ʱ����
dxgi.destroy()

