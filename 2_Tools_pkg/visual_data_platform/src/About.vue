<template>
  <div
    id="app"
    class="help"
  >
    <div style="margin: 40px auto 0 auto; width: 320px;">
      <div style="display:flex; align-items: center">
        <img
          width="60"
          height="60"
          src="./assets/icon.png"
        >
        <p style="margin-left:20px;font-size:28px; padding-top: 20px;">
          数据实验室
        </p>
      </div>
      <div>
        <span style="color: green;display: block;">
          {{ deActived?'下次使用需要重新激活':'已激活' }}
          <button
            v-if="!deActived"
            @click="deActive"
          >取消激活</button>
        </span>
        <span style="font-size:13px;">{{ validDate }}</span>
      </div>
      <!-- <p
        style="margin-top:60px;"
        class="info"
      >
        版权归上海早叶信息科技有限公司所有
      </p> -->
      <p class="info">
        技术支持请联系: wuwei_nero@163.com
      </p>
    </div>
  </div>
</template>

<script>
const Store = window.require('electron-store')
const store = new Store()

export default {
  data () {
    return {
      validDate: '',
      deActived: false
    }
  },

  mounted () {
    this.validDate = store.get('expiredAt')
  },

  methods: {
    deActive () {
      store.set('ACTIVE_KEY', '')
      this.deActived = true
    }
  }
}
</script>

<style lang="less">
.help {
  padding: 10px;
  font-size: 19px;

  .info {
    font-size: 14px;
  }
}
</style>
