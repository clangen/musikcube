const mappings = [
    /* basic Russian: https://github.com/clangen/musikcube/issues/613 */
    'а:А',
    'б:Б',
    'в:В',
    'г:Г',
    'д:Д',
    'е:Е',
    'ё:Ё',
    'ж:Ж',
    'з:З',
    'и:И',
    'й:Й',
    'к:К',
    'л:Л',
    'м:М',
    'н:Н',
    'о:О',
    'п:П',
    'р:Р',
    'с:С',
    'т:Т',
    'у:У',
    'ф:Ф',
    'х:Х',
    'ц:Ц',
    'ч:Ч',
    'ш:Ш',
    'щ:Щ',
    'ъ:Ъ',
    'ы:Ы',
    'ь:Ь',
    'э:Э',
    'ю:Ю',
    'я:Я',
];

String.prototype.toHex = function () {
  var result = '';
  for (var i = 0; i < this.length; i++) {
    result += this.charCodeAt(i).toString(16);
  }
  return '0x' + result;
};

for (let i = 0; i < mappings.length; i++) {
  const parts = mappings[i].split(':');
  parts[1] = [parts[1], parts[0]];
  console.log(`{ (u32)${parts[0].toHex()} /* ${parts[0]} */, u8"${parts[1].join('')}" },`);
}
