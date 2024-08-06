import ctypes

# 定义MessageBoxA函数
MessageBox = ctypes.windll.user32.MessageBoxW

while True:
    # 弹出MessageBox
    MessageBox(None, "Hello, this is a message box!", "Message Box Title", 0)
