"use strict";

// Optionally create hex2, hex4, mksigned8, and save_js if needed...
if (typeof hex2 !== 'function') {
    /**
     * @param {Number} val
     */
    window.hex2 = function(val) {
        let outstr = val.toString(16);
        if (outstr.length === 1) outstr = '0' + outstr;
        return outstr.toUpperCase();
    }
}

if (typeof hex4 !== 'function') {
    /**
     * @param {Number} val
     */
    window.hex4 = function(val) {
        let outstr = val.toString(16);
        if (outstr.length < 4) outstr = '0' + outstr;
        if (outstr.length < 4) outstr = '0' + outstr;
        if (outstr.length < 4) outstr = '0' + outstr;
        return outstr.toUpperCase();
    }
}

if (typeof mksigned8 !== 'function') {
    window.mksigned8 = function(what) {
        return what >= 0x80 ? -(0x100 - what) : what;
    }
}

var GENTARGET = 'c';
var GENEQO = '==';


/**
 * @param {Number} val
 */
function hex0x2(val) {
    return '0x' + hex2(val);
}

/**
 * @param {Number} val
 */
function hex0x4(val) {
    return '0x' + hex4(val);
}

// https://stackoverflow.com/questions/3665115/how-to-create-a-file-in-memory-for-user-to-download-but-not-through-server
function save_js(filename, data, kind = 'text/javascript') {
    const blob = new Blob([data], {type: kind});
    if(window.navigator.msSaveOrOpenBlob) {
        window.navigator.msSaveBlob(blob, filename);
    }
    else{
        const elem = window.document.createElement('a');
        elem.href = window.URL.createObjectURL(blob);
        elem.download = filename;
        document.body.appendChild(elem);
        elem.click();
        document.body.removeChild(elem);
    }
}


var seed_input, numtests_input;
window.onload = function() {
    //seed_input = document.getElementById('seed');
    //dconsole = new dct();
    //seed_input.value = "apples and oranges";
    //numtests_input = document.getElementById('numtests');
    //numtests_input.value = '1000';
}

class dct {
    constructor() {
        this.el = document.getElementById('statushere');
    }

    addl(order, what) {
        console.log('LOG:', what);
        this.el.innerHTML = what;
    }
}
var dconsole;

function generate_spc_tests() {
    let seed = seed_input.value;
    if (seed.length < 1) {
        alert('Please use a seed!');
        return;
    }
    let numtests = parseInt(numtests_input.value);
    if (!numtests) {
        alert('Please enter a valid number of tests to generate per opcode');
        return;
    }
    SPC_NUM_TO_GENERATE = numtests;
    generate_SPC700_tests(seed);
}

function set_gentarget(to) {
    GENTARGET = to; // JavaScript
    GENEQO = (GENTARGET === 'as') ? '==' : '===';
}

function generate_wdc65816_js() {
    set_gentarget('js');
	save_js('wdc65816_generated_opcodes.js', '"use strict";\n\nconst wdc65816_decoded_opcodes = Object.freeze(\n' + decode_opcodes() + ');');
}

function generate_spc700_js() {
    set_gentarget('js');
    save_js('spc700_generated_opcodes.js', '"use strict";\n\nconst SPC_decoded_opcodes = Object.freeze(\n' + SPC_decode_opcodes() + ');');
}

function generate_nes6502_js() {
    set_gentarget('js');
    save_js('nesm6502_generated_opcodes.js', generate_nes6502_core());
}

function generate_nes6502_as() {
    set_gentarget('as');
    save_js('nesm6502_generated_opcodes.ts', generate_sm83_core_as());
}

function generate_65c02_js() {
    set_gentarget('js');
    save_js('m65c02_generated_opcodes.js', generate_6502_cmos_core());
}

function generate_z80_js() {
    set_gentarget('js');
    save_js('z80_generated_opcodes.js', generate_z80_core(false));
}

function generate_z80_as() {
    set_gentarget('as');
    save_js('z80_generated_opcodes.ts', generate_z80_core_as(false));
}

function generate_sm83_js() {
    set_gentarget('js');
    save_js('sm83_generated_opcodes.js', generate_sm83_core());
}

function generate_sm83_as() {
    set_gentarget('as');
    save_js('sm83_generated_opcodes.ts', generate_sm83_core_as());
}

function generate_sm83_c() {
    set_gentarget('c');
    console.log('OK DOIJGN C');
    save_js('sm83_opcodes.cpp', generate_sm83_core_c());
}


function click_generate_sm83_tests() {
    let seed = seed_input.value;
    if (seed.length < 1) {
        alert('Please use a seed!');
        return;
    }
    let numtests = parseInt(numtests_input.value);
    if (!numtests) {
        alert('Please enter a valid number of tests to generate per opcode');
        return;
    }

    SM83_NUM_TO_GENERATE = numtests;
    generate_SM83_tests(seed);
}
function click_generate_z80_tests() {
    let seed = seed_input.value;
    if (seed.length < 1) {
        alert('Please use a seed!');
        return;
    }
    let CMOS = document.getElementById('Z80cmos').checked;
    let simplified_mem = document.getElementById('Z80simplmem').checked;
    let refresh = document.getElementById('Z80refresh').checked;
    let nullwaits = document.getElementById("Z80nullwait").checked;
    let numtests = parseInt(numtests_input.value);
    if (!numtests) {
        alert('Please enter a valid number of tests to generate per opcode');
        return;
    }

    Z80_DO_FULL_MEMCYCLES = !simplified_mem;
    Z80_DO_MEM_REFRESHES = refresh; // Put I/R on address bus
    SM83_NUM_TO_GENERATE = numtests;
    Z80_NULL_WAIT_STATES = nullwaits;
    generate_Z80_tests(seed, CMOS);
}