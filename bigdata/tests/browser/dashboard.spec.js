const { test, expect } = require('@playwright/test');

for (const viewport of [{ width: 1920, height: 1080 }, { width: 1366, height: 768 }]) {
  test(`dashboard remains usable at ${viewport.width}x${viewport.height}`, async ({ page }) => {
    await page.setViewportSize(viewport);
    const errors = [];
    page.on('console', message => { if (message.type() === 'error') errors.push(message.text()); });
    await page.goto(process.env.DASHBOARD_URL || 'http://127.0.0.1:5000');
    await expect(page.getByRole('heading', { name: '深圳充电运营大数据中心' })).toBeVisible();
    const overflow = await page.evaluate(() => document.documentElement.scrollWidth - document.documentElement.clientWidth);
    expect(overflow).toBeLessThanOrEqual(1);
    await page.getByRole('button', { name: '监管专题' }).click();
    await expect(page.getByRole('heading', { name: '模型漂移' })).toBeVisible();
    await page.getByRole('button', { name: '租户专题' }).click();
    await expect(page.getByRole('heading', { name: '扩容分析' })).toBeVisible();
    expect(errors).toEqual([]);
  });
}
