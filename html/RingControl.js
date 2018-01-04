/* javascript for Photo Ring Control */
/* process the button clicks into form submits */
function handleSelect () {
    // download everything everytime (for now)
    on = arg.value;
    onOff = document.getElementById("onOff").value;
    mode = document.getElementById("mode").value;
    brightness = document.getElementById("brightness").value;
    color = document.getElementById("color").value;
    direction = document.getElementById("direction").value;
    speed = document.getElementById("speed").value;
    valueToSend = onOff + "l" + brightness +
            color + "d" + direction + "s" + speed + "m" + mode;
    document.getElementById("arg").value = valueToSend;
    console.log ("sending: " + valueToSend);
    //document.getElementById("particle").submit();


    //var accessToken = "29acd3a806898af40f23c72bd30ab71518ae7b42";
    //var deviceId = "210027001047343438323536";
    var path = "https://api.particle.io/v1/devices/" + deviceId +
            "/ledControl?access_token=" + accessToken;
    var form = document.createElement("form");
    form.setAttribute( "method", "POST");
    form.setAttribute( "action", path);

    var hiddenField = document.createElement("input");
    hiddenField.setAttribute("type", "hidden");
    hiddenField.setAttribute("name", arg);
    hiddenField.setAttribute("value", valueToSend);
    form.appendChild(hiddenField);
    document.body.appendChild(form);
    form.submit();
}
