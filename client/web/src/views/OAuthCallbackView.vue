<script setup lang="ts">
import { onMounted, ref } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { Button, showFailToast } from 'vant'
import { useAuthStore } from '@/stores/auth'
import { clearPkceSession } from '@/auth/pkce'

const route = useRoute()
const router = useRouter()
const auth = useAuthStore()
const error = ref('')

onMounted(async () => {
  const oauthError = route.query.error as string | undefined
  if (oauthError) {
    error.value = (route.query.error_description as string) || oauthError
    clearPkceSession()
    return
  }

  const code = route.query.code as string | undefined
  const state = route.query.state as string | undefined
  if (!code || !state) {
    error.value = '缺少 OAuth 参数，请重新登录'
    return
  }
  try {
    await auth.handleCallback(code, state)
    const redirect = (route.query.redirect as string) || '/conversations'
    await router.replace(redirect)
  } catch (e) {
    error.value = e instanceof Error ? e.message : String(e)
    clearPkceSession()
    showFailToast(error.value)
  }
})

function retryLogin(): void {
  clearPkceSession()
  void router.replace('/login')
}
</script>

<template>
  <div class="callback">
    <template v-if="!error">
      <p>登录中…</p>
    </template>
    <template v-else>
      <p class="callback__error">{{ error }}</p>
      <p class="callback__hint">多半是登录中途刷新了页面，或换了访问地址。请重新点一次登录。</p>
      <Button type="primary" round @click="retryLogin">返回登录</Button>
    </template>
  </div>
</template>

<style scoped>
.callback {
  min-height: 100vh;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 12px;
  padding: 24px;
  color: #666;
  text-align: center;
}
.callback__error {
  color: #ee0a24;
  margin: 0;
}
.callback__hint {
  margin: 0;
  font-size: 13px;
  max-width: 320px;
}
</style>
