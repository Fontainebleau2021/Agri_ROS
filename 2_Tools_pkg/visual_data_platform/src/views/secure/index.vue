<template>
  <div class="login-page">
    <div class="header">
      请求码：{{ requestCode }}
      <a
        style="font-size:16px;"
        @click="copyCode(requestCode)"
      >
        复制
      </a>
    </div>
    <div class="login">
      <a-textarea
        v-model="responseCode"
        placeholder="输入激活秘钥"
        :auto-size="{ minRows: 5, maxRows: 10 }"
      >
        <a-icon
          slot="prefix"
          type="lock"
          style="color:rgba(0,0,0,.25)"
        />
      </a-textarea>
      <a-button
        style="margin-top:10px;"
        block
        @click="readFromFile"
      >
        从文件读取激活码
      </a-button>
      <a-button
        type="primary"
        style="margin-top:10px;"
        block
        @click="active"
      >
        激 活
      </a-button>
    </div>
  </div>
</template>

<script>
import vuex from '@/vuex'
const { readFileDialog } = require('@/utils/file')

export default {
  name: 'Login',

  data () {
    console.log(vuex.store.get('MACHINE_ID'))
    return {
      requestCode: vuex.store.get('MACHINE_ID'),
      responseCode: ''
    }
  },

  methods: {
    active () {
      if (this.responseCode === vuex.store.get('ACTIVE_KEY')) {
        this.$message.error('激活码错误!')
        return
      }
      vuex.store.set({
        'ACTIVE_KEY': this.responseCode,
        'CYCLE': btoa(0)
      })
      this.$router.push({
        name: 'develop'
      })
    },
    readFromFile () {
      readFileDialog({
        filters: [
          { name: 'All File', extensions: ['*'] }
        ]
      }).then(data => {
        this.responseCode = data
      })
    },

    copyCode (code) {
      this.$message.success('请求码已经复制到剪贴板')
      this.$clipboard(code)
    }
  }
}
</script>

<style lang="less">
.login-page {
  width: 100vw;
  height: 100vh;
  text-align: center;

  .header {
    user-select: text;
    padding-top: 20vh;
    width: 580px;
    margin: 0 auto;
    font-size: 22px;
    padding-bottom: 20px;
    word-break: break-all;
    text-align: left;
  }

  .login {
    width: 580px;
    margin: 0 auto;
    text-align: left;
  }
}
</style>

