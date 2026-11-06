import Vue from 'vue'
import VueRouter from 'vue-router'
import Layout from '@/layout'

Vue.use(VueRouter)

const routes = [
  {
    path: '/',
    redirect: '/develop'
  },
  {
    path: '/active',
    name: 'active',
    component: () => import('@/views/secure')
  },
  {
    path: '/develop',
    name: 'develop',
    component: Layout
  }
]

const router = new VueRouter({
  routes
})

export default router
