import vuex from '@/vuex'
import aes from 'crypto-js/aes'
import enc from 'crypto-js/enc-utf8'
import moment from 'moment'
import { message } from 'ant-design-vue'

const { machineIdSync } = window.require('node-machine-id')

export const addProtect = function (router) {
  const cycle = parseInt(atob(vuex.store.get('CYCLE') || 'MA=='))
  vuex.store.set({
    'MACHINE_ID': machineIdSync({ original: true }),
    'CYCLE': btoa(cycle + 1)
  })

  router.beforeEach((to, from, next) => {
    if (to.name === 'active') {
      next()
      return
    }

    // 已经生成过
    if (vuex.store.get('MACHINE_ID')) {
      const atk = vuex.store.get('ACTIVE_KEY')

      if (atk === 'TRIAL_KEY') {
        if (cycle > 600) {
          message.info('试用已到期，请激活!')
          next({ path: '/active' })
          return
        }

        const expiredAtDate = vuex.store.get('expiredAtDate')
        if (expiredAtDate) {
          const now = moment().valueOf()
          if (now > expiredAtDate) {
            message.info('激活码已经过期，请重新激活!')
            next({ path: '/active' })
            return
          }
        } else {
          const now = moment().add(30, 'days')
          vuex.store.set('expiredAtDate', now.valueOf())
        }

        vuex.store.set('expiredAt', `软件将于${moment(vuex.store.get('expiredAtDate')).format('YYYY-MM-DD')}日过期`)

        // 正确通过审核
        next()
        return
      }

      // 判断是否有秘钥
      if (atk) {
        // 判断秘钥是否过期
        const bytes = aes.decrypt(vuex.store.get('ACTIVE_KEY'), '19a3a2711b722756')
        try {
          const data = JSON.parse(bytes.toString(enc))
          // 判断机器码
          if (data.machineId !== vuex.store.get('MACHINE_ID')) {
            message.error('激活码错误!')
            next({ name: 'active' })
            return
          }
          // 时间过期
          const now = moment().valueOf()
          if (now > data.expiredAt) {
            message.info('激活码已经过期，请重新激活!')
            next({ path: '/active' })
            return
          }

          if (data.limitCycle && data.limitCycle < cycle) {
            message.info('激活码使用次数已达上限，请重新激活!')
            next({ path: '/active' })
            return
          }

          vuex.store.set('expiredAt', `软件将于${moment(data.expiredAt).format('YYYY-MM-DD')}日过期`)

          // 正确通过审核
          next()
        } catch (e) {
          message.error('激活码错误!')
          next({ path: '/active' })
          return
        }
      } else {
        message.info('使用软件前请先完成激活操作!')
        next({ path: '/active' })
        return
      }
    } else {
      vuex.store.set({
        'MACHINE_ID': machineIdSync({ original: true })
      })
      next({ path: '/active' })
      return
    }
  })
}
