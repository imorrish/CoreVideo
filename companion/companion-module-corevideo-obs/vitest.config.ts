import { defineConfig } from 'vitest/config'

export default defineConfig({
	test: {
		// Unit tests for pure logic only; no network or Companion runtime.
		include: ['src/**/*.test.ts'],
		environment: 'node',
	},
})
