function ConfigureSetupScreen() {
    // Configure Drop Down
    for (let i = 1; i <= 8; i++) {
        var dropdown = document.getElementById("button" + i + "act");
        AddButtonOptions(dropdown);
    }

    AddTimerOptions();
    GetDeviceOptions();
}

function GetDeviceOptions() {
    console.log("Getting Device Options");
    fetch('/GetOptions.json')
        .then(response => response.json())
        .then(data => {
            SetOptions(data);
            var parentForm = document.getElementById("setupdiv");
            parentForm.style.visibility = "visible";
        })
        .catch(error => console.error('Error fetching options:', error));
}

async function PostDeviceOptions() {
    let form = document.getElementById("setupform");
    let setupDiv = document.getElementById("setupdiv");
    var savingDiv = document.getElementById("savingdiv");
    setupDiv.style.visibility = "collapse";
    savingDiv.style.visibility = "collapse";

    let response = await fetch(form.action, {
        method: form.method,
        body: new FormData(form),
    });

    setupDiv.style.visibility = "visible";
    savingDiv.style.visibility = "visible";
}

function SetOptions(data) {
    console.log(data);
    document.getElementById("button1act").value = data.button1opt;
    document.getElementById("button2act").value = data.button2opt;
    document.getElementById("button3act").value = data.button3opt;
    document.getElementById("button4act").value = data.button4opt;
    document.getElementById("button5act").value = data.button5opt;
    document.getElementById("button6act").value = data.button6opt;
    document.getElementById("button7act").value = data.button7opt;
    document.getElementById("button8act").value = data.button8opt;
    document.getElementById("timermin").value = data.timermin;
    document.getElementById("timersec").value = data.timersec;
    document.getElementById("windhost").value = data.windhost || "";
}

function AddTimerOptions() {
    var minSelect = document.getElementById("timermin");
    for (let i = 0; i < 60; i++) {
        var option = document.createElement('option');
        option.value = i;
        option.innerHTML = i;
        minSelect.appendChild(option);
    }
    var secSelect = document.getElementById("timersec");
    for (let i = 0; i < 60; i++) {
        var option = document.createElement('option');
        option.value = i;
        option.innerHTML = i;
        secSelect.appendChild(option);
    }
}

async function SendControl(command) {
    let url = "/SendCommand.html?action=" + command;
    let response = await fetch(url);
}

function AddButtonOptions(dropdown) {
    var options = [
        { value: 0, text: "Minus One" },
        { value: 1, text: "Plus One" },
        { value: 2, text: "Minus Ten" },
        { value: 3, text: "Plus Ten" },
        { value: 4, text: "Auto" },
        { value: 5, text: "Stand By" },
        { value: 6, text: "Start Timer" },
        { value: 7, text: "Wind Mode" }
    ];

    options.forEach(function(opt) {
        var option = document.createElement('option');
        option.value = opt.value;
        option.innerHTML = opt.text;
        dropdown.appendChild(option);
    });
}
