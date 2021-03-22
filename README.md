
## Building

Vulkan validation layers can be installed from `https://vulkan.lunarg.com/sdk/home`

`git clone --recursive https://github.com/Reefufui/vulkan_image_processing.git`

`sh run.sh`


# ✔️Тривиальная проверка:

Запустить программу `./build/vulkan_denoice *path to image*`

# ОS:

Works fine on my Arch Linux machine

# ✔️Реализация на CPU и GPU

Биальтеральный реализован на CPU и GPU

Нелокальный реализован только на GPU

# ✔️ТИнформация в коммандной строке:

Время копирования и выполения шейдера (ns) для реализаций на GPU (Timestamps)

Время работы в секундах для CPU

# ✔️Реализованы два фильтра

Нелокальный и биальтеральный

# ✖️Учет допольнительных слоев

: ( не успел

# ✔️Учет соседних кадров

Это было смысл делать для нелокального фильтра - ведь чем больше похожих областей, тем лучше качество

Кол-во используемых кадров регулируется в исходном коде

# ✔️Перекрытие копирования и вычислений

Пока мы работаем с одним кадром - следующий уже копируется (в одном коммандном буффере)

Реализованно сменой DSetов - меням две текстуры (для копирования и для диспатча)

# ✔️Использование текстур

Имлеметирован режим для биальтерального фильтра в котором мы используем линейный буффер текселей, чтобы оценить разницу

# ✔️HHDR изображения

Поддерживается формат .exr (альфа канал сохраняется)
