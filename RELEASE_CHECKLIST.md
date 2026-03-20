# Release Checklist

- [ ] Version bumped and CHANGELOG.md updated
- [ ] Regenerate `sdkconfig.defaults` using `idf.py menuconfig && idf.py save-defconfig`
- [ ] Git tag created and pushed
- [ ] `idf.py fullclean build` artifacts uploaded
- [ ] Smoke test run on hardware
