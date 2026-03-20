# Extra components

This directory contains extra (optional) components to lvgl.
It's a good place for contributions as there are less strict expectations about the completeness and flexibility of the components here.

In other words, if you have created a complex widget from other widgets, or modified an existing widget with special events, styles or animations, or have a new feature that could work as a plugin to lvgl feel free to the share it here.

## How to contribute
- Create a pull request with your new content
- Please follow LVGL's coding style
- Add setter/getter functions in pair
- Update [lv_conf_template.h](https://github.com/lvgl/lvgl/blob/master/lv_conf_template.h)
- Add description in the docs
- Add [examples](https://github.com/lvgl/lvgl/tree/master/examples)
- Update the changelog
- Add yourself to the [Contributors](#contributors) section below.

## Ideas
Here some ideas as inspiration feel free to contribute with ideas too.
- New calendar headers
- Color picker with RGB and or HSV bars
- Ruler, horizontal or vertical with major and minor ticks and labels
- New list item types
- [Preloaders](https://www.google.com/search?q=preloader&sxsrf=ALeKk01ddA4YB0WEgLLN1bZNSm8YER7pkg:1623080551559&source=lnms&tbm=isch&sa=X&ved=2ahUKEwiwoN6d7oXxAhVuw4sKHVedBB4Q_AUoAXoECAEQAw&biw=952&bih=940)
- Drop-down list with a container to which content can be added
- 9 patch button: Similar to lv_imgbtn but 9 images for 4 corner, 4 sides and the center

## Contributors
- lv_animimg: @ZhaoQiang-b45475
- lv_span: @guoweilkd
- lv_menu: @HX2003
