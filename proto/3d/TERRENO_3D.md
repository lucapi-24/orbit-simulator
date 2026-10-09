# Terreno 3D — base validada y errores pendientes

Documento de trabajo para `proto/3d/Noise3D.hpp`. La mitad del ruido 3D está
**validado y funcionando**; lo que queda son **4 errores de composición**, no de
andamiaje. Está todo en el commit `001b945`.

- [1. Qué es esta cosa](#1-qué-es-esta-cosa)
- [2. Lo que ya funciona (no tirar)](#2-lo-que-ya-funciona-no-tirar)
- [3. Error 1 — el nivel del mar lo decide la octava más fina](#3-error-1--el-nivel-del-mar-lo-decide-la-octava-más-fina)
- [4. Error 2 — el "océano" puede salir por encima del nivel del mar](#4-error-2--el-océano-puede-salir-por-encima-del-nivel-del-mar)
- [5. Error 3 — `gain = 0.5` produce ruido, no cordillera](#5-error-3--gain--05-produce-ruido-no-cordillera)
- [6. Error 4 — no hay LOD de octavas](#6-error-4--no-hay-lod-de-octavas)
- [7. Orden de trabajo y dependencias](#7-orden-de-trabajo-y-dependencias)
- [8. Verificación](#8-verificación)
- [9. Decisiones abiertas](#9-decisiones-abiertas)
- [10. Lo que no hay que tocar](#10-lo-que-no-hay-que-tocar)

---

## 1. Qué es esta cosa

El objetivo es un planeta continuo y caminable: relieve que se ve bien desde
órbita **y** que tiene detalle real a escala de metros. El obstáculo para
conseguirlo es que el ruido 2D por rejillas (el viejo `HeightAt`/`NoiseAt`)
**no tiene frecuencia humana**: su celda más fina medía 590 km, así que un
chunk de 5×5 km caía entero dentro de una sola celda y salía plano como una
mesa. Subdividir la malla no arregla nada, porque los tres vértices de cada
triángulo leen el mismo valor.

La solución adoptada es evaluar el ruido sobre el **vector dirección
unitario** del vértice en vez de sobre UVs. Eso da tres cosas de golpe:

1. **Sin rejilla visible.** El campo es continuo por construcción, no hay
   celdas que delatar.
2. **Sin costura.** No hay UVs, luego no hay unión de textura.
3. **Sin compresión polar.** No hay proyección equirectangular, luego los polos
   no se estiran. Esto era justo la causa de la franja de agua ecuatorial del
   planeta 2D.

La misma función alimenta color, desplazamiento, colisión y el futuro chunk
local. Como es **la misma**, no pueden desincronizarse entre sí.

### La escala: por qué la frecuencia `f` lo controla todo

Al evaluar el ruido en `dir * f`, una celda mide en la superficie:

```
longitud de arco por celda  =  R / f
```

Es decir, **`f` es el inverso del tamaño del rasgo**, y `R = 6000 km`. Con
`FREC_BASE = 2` y `FBM_LACUN = 2`, la octava `k` tiene:

| Octava | `f` | `λ` |
|---|---|---|
| 0 | 2 | 3000 km |
| 4 | 32 | 187 km |
| 8 | 512 | 11.7 km |
| 12 | 8192 | 732 m |
| 16 | 131072 | 45 m |

17 octavas cubren de 3000 km a 45 m. Eso es el rango completo.

---

## 2. Lo que ya funciona (no tirar)

Verificado con `noise_test.exe` (headless, escribe PNG y números):

| Métrica | Valor | Referencia |
|---|---|---|
| Altura máxima | 9.25 km | Everest 8.8 km |
| Superficie de tierra | 27.0 % | Tierra real 29 % |
| Costura de UV (lon = ±π) | 1.9e-6 km | ~0, redondeo de float32 |
| Costura polar (delta al cruzar) | 0.020 km/km | sin costura |
| Delta máx. en paso de 10 m (27 rutas) | 0.0003 km | sin acantilados |
| Pendiente máx. (base 2 m, 53 000 sondas) | 0.7° | caminable |

Piezas correctas y reutilizables tal cual:

- `Hash3` — hash entero, sin `sin()`. `sin()` es impreciso en float32 y cambia
  entre máquinas; la aritmética entera es determinista.
- `Gradient` — los 12 gradientes de arista de cubo, conjunto clásico de Perlin.
- `Fade` — el polinomio quíntico `6t⁵-15t⁴+10t³`, que anula la derivada en los
  8 vértices de la celda. **Sin esto la rejilla se ve al ampliar.**
- `Noise3D` — interpolación trilineal de las 8 proyecciones de gradiente.
- La idea de evaluar sobre `dir` en vez de sobre UV (todo lo anterior).
- `Altitud3DEx(dir, contBias, norm)` — exponer los knobs para poder calibrar
  sin recompilar. Merece la pena conservarlo.
- `noise_test.cpp` con sus aserciones numéricas.

Dos bugs **reales** ya los cazó el test externo y ya están arreglados; conviene
recordarlos porque son el motivo de que el test exista:

- Un `early return` de océano creaba un **acantilado de 2.6 km en cada costa**,
  y el "océano" podía alcanzar +1.9 km. Se sustituyó por una mezcla continua
  `ocean*(1-mask) + h*mask`.
- Dos tests mentían: el paso "10 m" era en realidad 10 km (faltaba el factor
  1000 al convertir km→m), y la sonda de pendiente usaba una base de 30 m, que
  está **por debajo del límite de Nyquist** para la octava de 45 m, así que
  reportaba pendientes ~0 falsas.

---

## 3. Error 1 — el nivel del mar lo decide la octava más fina

**Este es el bug grande.** Es la causa de "islas en vez de continentes".

### El código actual

`Noise3D.hpp:156`

```c
return ocean * (1.0f - mask) + h * mask;
```

### Qué decide cada cosa

- `mask` decide el **carácter** del terreno: suelo marino frente a cordillera.
  Es un campo suave, de baja frecuencia. Correcto.
- El **nivel del mar** lo decide el signo de `h * mask`, y `h` es el fBm de
  **17 octavas con media cero**.

Y aquí está el problema: **en el interior de un continente `mask = 1`, así que
la elevación es simplemente `h`, y `h` es negativa la mitad de las veces.**

Es decir: la mitad de cada continente está por debajo del nivel del mar. La
costa no la dibuja el campo continental, la dibuja el **conjunto de ceros** de
una función fractal de 17 octavas.

### Por qué eso produce un archipiélago

El conjunto de ceros de un fBm no es una curva: es una curva **fractal**. A
cada escala de detalle le añade más muescas, indefinidamente. Un nivel del mar
definido así corta el terreno en trozos:

- Arriba en el mapa → muchas islas diminutas.
- En el interior de lo que debería ser un continente → lagos y bahías que en
  realidad son agujeros al océano abierto.

Que es exactamente lo que se vio: más islas que continentes.

### El arreglo

Que la costa la decida **solo el campo continental** (que es suave), y que el
detalle solo pueda *añadir* relieve encima, nunca volver a cruzar el nivel del
mar. La forma es partir la elevación en dos términos:

```c
base = (cont - CONT_LEVEL) / (1.0f - CONT_LEVEL)   // monótono en cont, 0 en la costa
elev = base * AMP_LAND  +  detalle * mask
```

`base` es monótono creciente en `cont` y vale exactamente 0 en el umbral del
mar, así que **por construcción la costa es el nivel cero de `cont`**, una curva
continental suave, no un fractal.

### La restricción que hay que respetar

Hay un dilema real aquí, no es un detalle de gusto:

El detalle `h` puede bajar hasta unos `-9 km`. Para que aparezca un lago
inland hace falta que `base * AMP_LAND` sea menor que 9 km. La franja de
`cont` donde eso pasa tiene anchura:

```
ancho de la franja de lagos  =  9 km · (1 - CONT_LEVEL) / AMP_LAND
```

- `AMP_LAND` **grande** → franja estrecha → casi nunca hay lagos, pero la altura
  máxima la marca `AMP_LAND`, no 9 km.
- `AMP_LAND` **pequeño** → franja ancha → la mitad del continente se inunda.

**No se puede tener a la vez "sin lagos" y "detalle de ±9 km junto a la
costa".** Es una decisión de estilo, y hay tres salidas limpias:

- **(a) Estricta:** el detalle es **multiplicativo** en `base`, o sea
  `elev = base * (AMP_LAND + K·h·mask)`. Entonces `elev > 0 ⟺ base > 0`
  exactamente, y por construcción **nunca** hay lago inland. Coste: el relieve
  crece hacia el interior, así que no hay montañas justo en la costa.
- **(b) Franja costera:** el detalle entra completo a partir de cierto `base`,
  aceptando que exista una banda costera fragmentada. Si esa banda sale
  estrecha, lee como archIPIélago — que es un **estilo legítimo**, no un bug.
  Solo hay que elegirlo a propósito, no heredarlo.
- **(c) Lagos explícitos:** un campo de lagos propio, de baja frecuencia, que se
  multiplica dentro del continente. Así los lagos son una **característica
  diseñada**, no un accidente de la aritmética.

Mi recomendación es **(a) o (c)**. La (b) es_DEFENSABLE solo si el objetivo
estético es justamente archIPIélago.

### Efecto colateral: la calibración queda invalidada

`CONT_BIAS = 0.25` y `NORMALIZACION = 0.4220` se calibraron contra la función
**con el bug**. El porcentaje de tierra medido por el test era
`elev > 0`, es decir, la **intersection** de dos campos, no `cont`. Al cambiar
la composición, ambos valores hay que **re-barrer** (ver §8).

---

## 4. Error 2 — el "océano" puede salir por encima del nivel del mar

`Noise3D.hpp:137`

```c
float ocean = -OCEAN_DEPTH
            - Noise3D(Vector3Scale(dir, 4.0f)) * 2.0f
            - Noise3D(Vector3Scale(dir, 9.0f)) * 0.9f;
```

El comentario del código dice *"Todo NEGATIVO por construcción"*. **Es falso.**

`Noise3D` tiene signo, y los términos van con signo **negativo** delante, así
que cuando el ruido sale negativo la resta se suma. Con el rango de Perlin
(±0.7):

```
ocean  ∈  [ -1 - 1.4 - 0.63 ,  -1 + 1.4 + 0.63 ]  =  [ -3.03 , +1.03 ] km
```

Hasta **+1.03 km de "fondo marino" por encima del nivel del mar**. Esas motas
son islas diminutas dispersas, y el test las contaba como tierra (porque cuenta
`elev > 0`).

### El arreglo

Que la corrección sea por **magnitud**, no con signo:

```c
ocean = -OCEAN_DEPTH - (fabsf(n1) * 2.0f + fabsf(n2) * 0.9f);
```

Con `fabs` el rango es `[-3.03, -1.00]`: garantizado negativo **por
construcción**, que es lo que decía el comentario.

Es un cambio de una línea, pero ojo: **el porcentaje de tierra bajará un poco**
al arreglarlo (desaparecen las islas-falsa), así que `CONT_BIAS` hay que
re-calibrar después.

---

## 5. Error 3 — `gain = 0.5` produce ruido, no cordillera

`Noise3D.hpp:26`

### La matemática

La pendiente que aporta una octava es proporcional a `amp × frec`:

```
pendiente_k  ∝  AMP · gain^k · f_0 · lac^k
```

Con `gain = 0.5` y `lac = 2`:

```
gain^k · lac^k = (0.5 · 2)^k = 1
```

**Cada octava aporta exactamente la misma pendiente.** Eso es lo que significa
"fractal auto-afín" y es la estadística del relieve real de la Tierra... y
también es exactamente el motivo de que **se lea como ruido**.

El problema es perceptual, no matemático: la octava de 45 m aporta la misma
pendiente que la de 3000 km, pero visualmente **domina**, porque hay 16 de
ellas y solo una grande. El ojo lee grano, no estructura.

### El arreglo

Bajar `gain` para que la pendiente **decaiga** con la frecuencia:

```
pendiente_k  ∝  (gain · lac)^k
```

Con `gain = 0.45`: ratio `0.45 · 2 = 0.9` por octava, así que la octava 16
aporta `0.9^16 ≈ 0.19` de lo que aporta la octava 0. Las formas grandes mandan y
las pequeñas quedan como textura. Ese es el ajuste fino entre "montaña" y
"ruido".

Hay un techo y un suelo: si `gain` es demasiado bajo, las octavas finales dejan
de verse y el terreno se vuelve blando a ras de suelo, perdiendo el detalle
caminable. Es un equilibrio, y hay que buscarlo mirando.

**Ojo:** cambiar `gain` cambia la amplitud total acumulada, así que
`NORMALIZACION` hay que re-calaribrar.

### Complemento opcional: ruido *ridged* (aristas)

Perlin fBm da colinas redondeadas. Las cordilleras reales tienen **crestas
afiladas y valles en V**, y eso sale de una transformación distinta: en vez de
usar el ruido tal cual, se usa `1 - |n|` (elevado al cuadrado si quieres), que
concentra los valores altos donde el ruido **cruza cero**, y de ahí salen las
líneas de cresta.

Hay tres "sabores" posibles, y elegir uno es una decisión de estilo:

| Sabor | Transformación | Resultado |
|---|---|---|
| Billow | `\|n\|` | lomas redondeadas, valles amplios |
| fBm puro | `n` | colinas suaves, ni crestas ni fosas |
| Ridged | `(1 - \|n\|)²` | crestas afiladas, valles en V, muy parecido a montañas |

"Ridged" es lo más parecido a una cordillera real. Pero cambia las
estadísticas, así que necesita su propia calibración. **No es obligatorio** para
arreglar el bug 3 — bajar `gain` ya corrige el síntoma de "parece ruido".

---

## 6. Error 4 — no hay LOD de octavas

`Altitud3D` no acepta número de octavas: siempre hace 17.

### Por qué está mal

Cada representación del terreno tiene un **límite de Nyquist** distinto, y
pedirle 17 octavas es excessivo para las tres por motivos distintos:

| Representación | Separación | λ mínimo representable | Octavas que necesita |
|---|---|---|---|
| Textura 2048×1024 | 18.4 km/texel | 36.8 km | **7** |
| Malla global 256×128 | 147 km/vértice | 294 km | **4** |
| Chunk 5 km con 256² | 19.5 m/vértice | 39 m | **17** |

- La **textura** se sobremuestrea 2.5×: octavas que caen por debajo del texel
  solo aportan aliasing.
- La **malla global** se sobremuestrea 60×: 147 km entre vértices no puede
  representar ni una montaña de 375 km con forma, y sin embargo se le están
  pidiendo rasgos de 45 m. El resultado no es "más detalle": es **ruido
  aliaseado** que *parece* detalle. Esto es una parte de lo que se vio en el
  perfil.
- El **chunk** sí justifica las 17: 19.5 m de separación resuelve los 45 m
  (Nyquist pide ≤22.9 m).

### El arreglo

Pasar `octaves` como parámetro y que **cada consumidor pida lo que puede
representar**. La firma natural:

```c
Altitud3D(dir, octaves)
```

Y encadenar: malla global `4`, textura `7`, chunk `17`.

### La consecuencia difícil: el popping

Si el chunk tiene 45 m de detalle y la malla global solo 294 km, al caminar
verás aparecer y desaparecer detalle según la distancia. Eso es **popping de
LOD**, y hay que planearlo. La salida estándar es:

- El chunk se dibuja cerca y la malla global lejos, con una transición por
  distancia o por altura.
- O la malla global **no se dibuja** cuando estás en superficie (la malla del
  chunk la tapa), y solo entra al alejarte.

Nota: el near-plane adaptativo que ya hay (`0.001` cerca, `0.1` en órbita) es un
parche related pero no resuelve el popping — resuelve el *z-fighting*.

---

## 7. Orden de trabajo y dependencias

| # | Qué | Depende de | Por qué en este orden |
|---|---|---|---|
| 1 | **Error 1** (costa continental) | — | Es el grande. Reorganiza la composición de `elev` y todo lo demás depende de cómo quede. |
| 2 | **Error 2** (signo del océano) | — | Una línea, independiente. Se puede hacer en cualquier momento. |
| 3 | **Re-calibrar** `CONT_BIAS` / `NORMALIZACION` | 1, 2 | Ambos cambian el porcentaje de tierra y la altura máxima. |
| 4 | **Error 3** (`gain` → 0.45) | 3 | Toca la amplitud acumulada, así que necesita la calibración al día. |
| 5 | **Decidir ridged o no** | 4 | Opcional. Si sí, cambia estadísticas → re-calibrar de nuevo. |
| 6 | **Error 4** (`octaves` por consumidor) | 1, 2 | Necesita la firma estable de `Altitud3D` para poder pasar el parámetro. |
| 7 | **Chunk local** | 4, 6 | Ya solo esiggneración: malla + colisión. Es la recompensa. |

Los errores 1 y 2 son independientes entre sí y se pueden hacer en cualquier
orden. El 3 tiene efecto acumulativo con el resto: **calibra siempre después de
cambiar las constantes.**

---

## 8. Verificación

El test externo ya existe y se ejecuta sin abrir el juego:

```
cmake --build build
.\build\noise_test.exe
```

Escribe dos PNG en `proto/3d/out/` (ignorado por git) y saca estas métricas:

- `TestPerfil` → `perfil_1d.png` (perfil de un gran círculo) + nº de cambios de
  pendiente. **Ojo:** con 2000 muestras sobre 37 699 km hay 19 km por píxel, así
  que el PNG mete 14 octavas por debajo de la resolución del dibujo. Es una
  imagen engañosa por construcción. Para juzgar forma hay que subir la
  resolución o el rango.
- `TestMapa` → `mapa_512.png` + % de tierra. 18.4 km/texel, así que los detalles
  de 45 m tampoco son visibles aquí. **Es la vista buena para juzgar
  continentes, islas y costas**, que es justo lo que se necesitaba.
- `TestContinuidad` → costura, polo, delta a paso de 10 m, pendiente máxima.
  Numérico, y es el que de verdad importa: caza acantilados y costuras que
  el ojo no ve.
- `TestCalibracion` → barre `contBias` de −0.20 a +0.70 y dice qué `norm`
  hace falta para 9 km. **Hay que re-correrlo después de cada cambio** en §3, §4
  o §5.

### Aserciones que faltarían

Para cerrar del todo el error 1 hacen falta dos checks nuevos que ahora mismo no
existen:

- **Fracción de tierra lacunar inland.** Si tras el arreglo sigue habiendo
  archIPIélago, es porque `AMP_LAND` es demasiado pequeño. Se mide contando
  componentes conexas de `elev > 0` y viendo cuántas hay: un continente son
  poquísimos componentes grandes; un archIPIélago son miles de pequeños.
- **Continuidad de la línea de costa.** Recorrer la costa y medir el radio de
  curvatura. Un nivel del mar definido por un fractal da radios de curvatura
  →0 en todas partes; uno definido por un campo suave da curvas con radio
  mínimo medible.

---

## 9. Decisiones abiertas

Cosas que **aún no** están decididas y que conviene tener presentes:

1. **Firma de `Altitud3D`.** ¿Lleva `octaves` como parámetro (como propone
   `Altitud3DEx` con sus knobs) o se resuelve con una constante aparte por
   representación? Afecta a la ergonomía de todo lo que llame a la función.
2. **Ridged o billow.** Estilo de montaña. Cambia el carácter visual más que
   cualquier otra constante.
3. **Lagos.** ¿Estricto (nunca), franja costera, o campo de lagos explícito?
   Afecta al porcentaje de tierra y a la lectura del mapa.
4. **Cuánta fractura darle a la costa.** El nivel de mar ahora lo dibuja un
   campo de 2 octavas (λ 4615 y 1923 km). Meter 2-3 octavas más en `cont` da
   fiordos y penínsulas; meter muchas más lo convierte otra vez en
   archIPIélago. **Este es el mejor tirador de "islas vs continentes"**, mejor
   que tocar el ancho de la máscara.
5. **CPU por vértice o GPU a textura.** Por vértice es sencillo y basta
   mientras el chunk sea pequeño. Si el chunk crece a 100×100, evaluar
   17 octavas por vértice en CPU se nota, y el sitio natural es un shader que
   calcule el height field una vez a textura. **No urge decidirlo hoy**, pero
   condiciona la firma.
6. **Tamaño y teselado del chunk.** Sale de §6: la combinación
   (tamaño chunk × vértices) es la que fija cuántas octavas son representables.
   Con 5 km y 256² salen las 17; con 20 km y 256² solo 12.

---

## 10. Lo que no hay que tocar

- `Hash3`, `Gradient`, `Fade`, `Noise3D` y la interpolación trilineal. Validados
  y correctos.
- La propiedad de evaluar sobre `dir` y **no** sobre UVs. De ahí salen la
  ausencia de costura, la ausencia de rejilla visible y la ausencia de
  compresión polar. Cualquier refactor que reintroduzca UVs pierde las tres.
- La mezcla continua `ocean*(1-mask) + h*mask` como estructura. Se puede cambiar
  la forma, pero **no volver a un `if` de dos ramas**: eso ya costó un
  acantilado de 2.6 km en cada costa.
- `Altitud3DEx` exponiendo los knobs. Permite calibrar sin recompilar, que es
  lo que hace posible el barrido.
- `noise_test.cpp` y sus aserciones numéricas. Es la razón de que este
  documento exista en vez de cuatro bugs en el log.
