var str = "请选择需学习的按键再次接收红外信号成功失败"
var strOut = ""
for(var i = 0, len = str.length; i < len; i++) strOut += ("0x" + str.charCodeAt(i).toString(16) + ",")
console.log(strOut)