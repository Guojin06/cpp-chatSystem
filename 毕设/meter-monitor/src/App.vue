<script setup>
import { ref, onMounted, onUnmounted } from 'vue'
 
const API_BASE = 'http://192.168.199.128:8000'

const devices = ref([
  { id: '000000000001', name: '1号电表', status: 'unknown', address: '000000000001' }
])
const selectedDevice = ref('000000000001')
const currentData = ref({
  timestamp: '',
  voltage: { A: 0, B: 0, C: 0 },
  current: { A: 0, B: 0, C: 0 },
  activePower: 0,
  reactivePower: 0,
  powerFactor: 0,
  forwardActiveEnergy: 0,
  reverseActiveEnergy: 0,
  forwardReactiveEnergy: 0,
  reverseReactiveEnergy: 0,
  maxDemand: 0,
  maxDemandTime: '-'
})
let timer = null

const selectDevice = (deviceId) => {
  selectedDevice.value = deviceId
}

const normalizeLatestData = (payload) => {
  const voltage = Number(payload?.data?.voltage ?? 0)
  const current = Number(payload?.data?.current ?? 0)
  const activeEnergy = Number(payload?.data?.active_energy ?? 0)

  currentData.value = {
    timestamp: payload?.timestamp ?? '',
    voltage: { A: voltage, B: 0, C: 0 },
    current: { A: current, B: 0, C: 0 },
    activePower: Number((voltage * current).toFixed(2)),
    reactivePower: 0,
    powerFactor: Number(payload?.data?.power_factor ?? 0),
    forwardActiveEnergy: activeEnergy,
    reverseActiveEnergy: 0,
    forwardReactiveEnergy: 0,
    reverseReactiveEnergy: 0,
    maxDemand: Number((voltage * current).toFixed(2)),
    maxDemandTime: payload?.timestamp ?? '-'
  }
}

const fetchConfig = async () => {
  try {
    const response = await fetch(`${API_BASE}/api/config`)
    if (!response.ok) {
      return
    }
    const config = await response.json()
    const meterId = config.meter_id ?? '000000000001'
    devices.value = [
      {
        id: meterId,
        name: '1号电表',
        status: 'unknown',
        address: meterId,
      }
    ]
    selectedDevice.value = meterId
  } catch (error) {
    console.error(error)
  }
}

const fetchLatest = async () => {
  try {
    const response = await fetch(`${API_BASE}/api/latest`)
    if (!response.ok) {
      throw new Error('fetch latest failed')
    }
    const payload = await response.json()
    console.log('API返回:', payload)  // 调试
    const meterId = payload?.meter_id ?? selectedDevice.value
    devices.value = devices.value.map(device => ({
      ...device,
      id: meterId,
      address: meterId,
      status: payload?.status === 'ok' ? 'online' : 'offline'
    }))
    selectedDevice.value = meterId
    normalizeLatestData(payload)
    console.log('处理后:', currentData.value)  // 调试
  } catch (error) {
    console.error('获取数据失败:', error)  // 调试
    devices.value = devices.value.map(device => ({
      ...device,
      status: 'offline'
    }))
  }
}

onMounted(() => {
  fetchConfig()
  fetchLatest()
  timer = setInterval(fetchLatest, 2000)
})

onUnmounted(() => {
  if (timer) clearInterval(timer)
})
</script>

<template>
  <el-container class="main-container">
    <el-header class="header">
      <h1>DTU+645 电表数据采集网关监控平台</h1>
    </el-header>
    
    <el-container>
      <el-aside width="280px" class="sidebar">
        <h3>设备列表</h3>
        <el-card 
          v-for="device in devices" 
          :key="device.id"
          :class="['device-card', selectedDevice === device.id ? 'active' : '']"
          @click="selectDevice(device.id)"
        >
          <div class="device-info">
            <div class="device-name">{{ device.name }}</div>
            <el-tag :type="device.status === 'online' ? 'success' : 'danger'" size="small">
              {{ device.status === 'online' ? '在线' : '离线' }}
            </el-tag>
          </div>
          <div class="device-id">设备ID: {{ device.id }}</div>
          <div class="device-addr">645地址: {{ device.address }}</div>
        </el-card>
      </el-aside>

      <el-main>
        <el-card class="data-card">
          <template #header>
            <div class="card-header">
              <span>实时数据监控</span>
              <el-tag type="info">更新时间: {{ currentData.timestamp }}</el-tag>
            </div>
          </template>

          <el-row :gutter="20">
            <el-col :span="8">
              <div class="stat-item">
                <div class="stat-label">A相电压</div>
                <div class="stat-value">{{ currentData.voltage.A.toFixed(1) }} V</div>
              </div>
            </el-col>
            <el-col :span="8">
              <div class="stat-item">
                <div class="stat-label">B相电压</div>
                <div class="stat-value">{{ currentData.voltage.B.toFixed(1) }} V</div>
              </div>
            </el-col>
            <el-col :span="8">
              <div class="stat-item">
                <div class="stat-label">C相电压</div>
                <div class="stat-value">{{ currentData.voltage.C.toFixed(1) }} V</div>
              </div>
            </el-col>
          </el-row>

          <el-divider />

          <el-row :gutter="20">
            <el-col :span="8">
              <div class="stat-item">
                <div class="stat-label">A相电流</div>
                <div class="stat-value">{{ currentData.current.A.toFixed(2) }} A</div>
              </div>
            </el-col>
            <el-col :span="8">
              <div class="stat-item">
                <div class="stat-label">B相电流</div>
                <div class="stat-value">{{ currentData.current.B.toFixed(2) }} A</div>
              </div>
            </el-col>
            <el-col :span="8">
              <div class="stat-item">
                <div class="stat-label">C相电流</div>
                <div class="stat-value">{{ currentData.current.C.toFixed(2) }} A</div>
              </div>
            </el-col>
          </el-row>

          <el-divider />

          <el-row :gutter="20">
            <el-col :span="8">
              <div class="stat-item">
                <div class="stat-label">有功功率</div>
                <div class="stat-value">{{ currentData.activePower.toFixed(2) }} W</div>
              </div>
            </el-col>
            <el-col :span="8">
              <div class="stat-item">
                <div class="stat-label">无功功率</div>
                <div class="stat-value">{{ currentData.reactivePower.toFixed(2) }} Var</div>
              </div>
            </el-col>
            <el-col :span="8">
              <div class="stat-item">
                <div class="stat-label">功率因数</div>
                <div class="stat-value">{{ currentData.powerFactor.toFixed(2) }}</div>
              </div>
            </el-col>
          </el-row>
        </el-card>

        <el-card class="data-card" style="margin-top: 20px;">
          <template #header>
            <span>电能数据</span>
          </template>

          <el-descriptions :column="2" border>
            <el-descriptions-item label="正向有功电能">
              {{ currentData.forwardActiveEnergy }} kWh
            </el-descriptions-item>
            <el-descriptions-item label="反向有功电能">
              {{ currentData.reverseActiveEnergy }} kWh
            </el-descriptions-item>
            <el-descriptions-item label="正向无功电能">
              {{ currentData.forwardReactiveEnergy }} kVarh
            </el-descriptions-item>
            <el-descriptions-item label="反向无功电能">
              {{ currentData.reverseReactiveEnergy }} kVarh
            </el-descriptions-item>
            <el-descriptions-item label="最大需量">
              {{ currentData.maxDemand }} W
            </el-descriptions-item>
            <el-descriptions-item label="最大需量发生时间">
              {{ currentData.maxDemandTime }}
            </el-descriptions-item>
          </el-descriptions>
        </el-card>
      </el-main>
    </el-container>
  </el-container>
</template>

<style scoped>
.main-container {
  height: 100vh;
}

.header {
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  color: white;
  display: flex;
  align-items: center;
  box-shadow: 0 2px 12px rgba(0,0,0,0.1);
}

.header h1 {
  margin: 0;
  font-size: 24px;
}

.sidebar {
  background: #f5f7fa;
  padding: 20px;
  overflow-y: auto;
}

.sidebar h3 {
  margin-top: 0;
  color: #303133;
}

.device-card {
  margin-bottom: 12px;
  cursor: pointer;
  transition: all 0.3s;
}

.device-card:hover {
  transform: translateY(-2px);
  box-shadow: 0 4px 12px rgba(0,0,0,0.15);
}

.device-card.active {
  border: 2px solid #409eff;
}

.device-info {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 8px;
}

.device-name {
  font-weight: bold;
  font-size: 16px;
}

.device-id, .device-addr {
  font-size: 12px;
  color: #909399;
  margin-top: 4px;
}

.data-card {
  margin-bottom: 20px;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.el-divider {
  margin: 30px 0;
}

.stat-item {
  text-align: center;
  padding: 16px;
  background: linear-gradient(135deg, #f8f9fa 0%, #e9ecef 100%);
  border-radius: 12px;
  transition: transform 0.2s;
}

.stat-item:hover {
  transform: scale(1.02);
}

.stat-label {
  font-size: 14px;
  color: #606266;
  margin-bottom: 8px;
}

.stat-value {
  font-size: 28px;
  font-weight: bold;
  color: #303133;
}
</style>
