// 模拟电表设备数据
export const mockDevices = [
  { id: 'METER_001', name: '1号电表', status: 'online', address: '000000000001' },
  { id: 'METER_002', name: '2号电表', status: 'online', address: '000000000002' },
  { id: 'METER_003', name: '3号电表', status: 'offline', address: '000000000003' },
  { id: 'METER_004', name: '4号电表', status: 'online', address: '000000000004' },
]

// 模拟实时电表数据
export const mockMeterData = {
  'METER_001': {
    deviceId: 'METER_001',
    timestamp: new Date().toLocaleString(),
    voltage: { A: 220.5, B: 219.8, C: 221.2 },
    current: { A: 10.5, B: 9.8, C: 11.2 },
    activePower: 6850,
    reactivePower: 1250,
    powerFactor: 0.92,
    forwardActiveEnergy: 12580.5,
    reverseActiveEnergy: 125.3,
    forwardReactiveEnergy: 3280.2,
    reverseReactiveEnergy: 85.6,
    maxDemand: 8500,
    maxDemandTime: '2025-01-05 14:30:00'
  },
  'METER_002': {
    deviceId: 'METER_002',
    timestamp: new Date().toLocaleString(),
    voltage: { A: 218.5, B: 220.1, C: 219.5 },
    current: { A: 8.2, B: 7.9, C: 8.5 },
    activePower: 5420,
    reactivePower: 980,
    powerFactor: 0.89,
    forwardActiveEnergy: 9850.2,
    reverseActiveEnergy: 68.5,
    forwardReactiveEnergy: 2150.8,
    reverseReactiveEnergy: 42.3,
    maxDemand: 6800,
    maxDemandTime: '2025-01-05 13:15:00'
  },
  'METER_003': {
    deviceId: 'METER_003',
    timestamp: '2025-01-05 12:00:00',
    voltage: { A: 0, B: 0, C: 0 },
    current: { A: 0, B: 0, C: 0 },
    activePower: 0,
    reactivePower: 0,
    powerFactor: 0,
    forwardActiveEnergy: 5230.8,
    reverseActiveEnergy: 0,
    forwardReactiveEnergy: 1080.5,
    reverseReactiveEnergy: 0,
    maxDemand: 0,
    maxDemandTime: '-'
}

// 模拟数据自动更新
export function getRandomData(deviceId) {
  const base = mockMeterData[deviceId]
  if (!base || base.voltage.A === 0) return base
  
  return {
    ...base,
    timestamp: new Date().toLocaleString(),
    voltage: {
      A: (220 + Math.random() * 5).toFixed(1),
      B: (220 + Math.random() * 5).toFixed(1),
      C: (220 + Math.random() * 5).toFixed(1)
    },
    current: {
      A: (10 + Math.random() * 2).toFixed(1),
      B: (10 + Math.random() * 2).toFixed(1),
      C: (10 + Math.random() * 2).toFixed(1)
    },
    activePower: Math.floor(6500 + Math.random() * 1000),
    powerFactor: (0.88 + Math.random() * 0.08).toFixed(2)
  }
}
