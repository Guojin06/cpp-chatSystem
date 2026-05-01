#!/usr/bin/env python3
"""
电表模拟器：往虚拟串口发送 DL/T 645 报文
用法：python3 meter_simulator.py /tmp/vserial0
"""
import sys
import time
import serial  # pip install pyserial

def build_645_frame():
    """构造一帧 645 应答报文（与 C++ buildMockFrame 完全一致）"""
    frame = bytearray([
        0x68,                                    # 帧头
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00,      # 地址域
        0x68,                                    # 第二个帧头
        0x91,                                    # 控制码
        0x0A,                                    # 数据域长度 = 10
        # 数据域（已加 0x33）
        0x33, 0x33, 0x34, 0x35,                  # 数据标识
        0x55, 0x35,                              # 电压
        0x45, 0x33,                              # 电流
        0x88, 0x43,                              # 有功电能
        0x00,                                    # 校验码占位
        0x16                                     # 帧尾
    ])
    # 计算校验码
    checksum = sum(frame[:-2]) & 0xFF
    frame[-2] = checksum
    return bytes(frame)

def main():
    port = sys.argv[1] if len(sys.argv) > 1 else "/tmp/vserial0"
    print(f"模拟器启动，写入端口: {port}")

    ser = serial.Serial(port, baudrate=9600, timeout=1)
    try:
        while True:
            frame = build_645_frame()
            ser.write(frame)
            print(f"已发送 {len(frame)} 字节: {frame.hex(' ')}")
            time.sleep(5)
    except KeyboardInterrupt:
        print("\n模拟器停止")
    finally:
        ser.close()

if __name__ == "__main__":
    main()
