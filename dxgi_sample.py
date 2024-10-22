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
windowTitle = 'OBS 30.2.2 - Profile: game - Scenes: Untitled'
# windowTitle = '���׽���'
hwnd = win32gui.FindWindow(None, windowTitle)
windll.user32.SetProcessDPIAware()
# time.sleep(10)
# ��ʼ��
dxgi.init_dxgi(hwnd)

# ָ����ͼ����(����ʾ��Ϊ��ȡ��������)
left, top, right, bottom = GetWindowRect(hwnd)
shotLeft, shotTop = 0, 0
height = bottom - top
width = right - left
# ����numpy array���ڽ��ս�ͼ���
shot = np.ndarray((height, width, 4), dtype=np.uint8)
shotPointer = shot.ctypes.data_as(POINTER(c_ubyte))
startTime = time.time()
# ��ͼ
for i in range(0, 60):
    buffer = dxgi.grab(shotPointer, shotLeft, shotTop, width, height)
    # ��ȡ���
    image = np.ctypeslib.as_array(buffer, shape=(height, width, 4))
    cv2.imwrite('test_dxgi/sample_pic' + str(i) + '.png', image)
endTime = time.time()
print('time cost: ' + str(endTime - startTime))
# תΪBGR��ʽ
img = cv2.cvtColor(image, cv2.COLOR_BGRA2BGR)
cv2.imshow('sample_pic', img)
cv2.waitKey(0)
# ����ʹ��ʱ����
dxgi.destroy()