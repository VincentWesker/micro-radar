const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const { test } = require('node:test');

const source = fs.readFileSync(path.join(__dirname, '..', 'src', 'ConfigurationWebServer.cpp'), 'utf8');
const script = source.match(/<script>([\s\S]*?)<\/script>/)[1].replace('%MAX_INFO_FIELDS%', '3');

function createPage(latitude = 51.5, radius = 1) {
    const fields = Array.from({ length: 6 }, (_, index) => ({
        checked: index < 3,
        disabled: false,
        parentElement: { classList: { toggle() {} } },
        addEventListener(event, handler) { this.change = handler; }
    }));
    const result = {};
    const distance = {};
    const latitudeInput = {
        valueAsNumber: latitude, validity: { valid: true },
        addEventListener(event, handler) { this.input = handler; }
    };
    const radiusInput = {
        valueAsNumber: radius, validity: { valid: true },
        addEventListener(event, handler) { this.input = handler; }
    };
    const form = { addEventListener(event, handler) { this.submit = handler; } };
    let saves = 0;
    vm.runInNewContext(script, {
        document: {
            querySelectorAll: () => fields,
            querySelector: selector => selector === 'input[name="latitude"]' ? latitudeInput : radiusInput,
            getElementById: id => id === 'cfg' ? form : id === 'radius-distance' ? distance : result
        },
        FormData: class {},
        fetch: () => { ++saves; return Promise.resolve({ text: () => 'Saved' }); }
    });
    return { fields, form, result, distance, latitudeInput, radiusInput, saves: () => saves };
}

test('three defaults disable other fields; deselecting unlocks alternatives', () => {
    const { fields } = createPage();
    assert.deepEqual(fields.map(field => field.disabled), [false, false, false, true, true, true]);
    fields[1].checked = false;
    fields[1].change();
    assert.ok(fields.every(field => !field.disabled));
    fields[3].checked = true;
    fields[3].change();
    assert.deepEqual(fields.map(field => field.disabled), [false, true, false, false, true, true]);
});

test('all 64 field combinations enforce the limit, including empty selection', () => {
    for (let mask = 0; mask < 64; ++mask) {
        const page = createPage();
        page.fields.forEach((field, index) => { field.checked = Boolean(mask & (1 << index)); });
        page.fields[0].change();
        const selected = page.fields.filter(field => field.checked).length;
        page.fields.forEach(field => assert.equal(field.disabled, selected >= 3 && !field.checked));
        page.form.submit({ preventDefault() {} });
        assert.equal(page.saves(), selected <= 3 ? 1 : 0);
        if (selected > 3) assert.match(page.result.textContent, /no more than 3/);
    }
});

test('radius estimate uses latitude, degrees, and the current radius', () => {
    for (const [latitude, radius, expected] of [
        [0, 1, '111.3 km each way north/south; 111.3 km each way east/west'],
        [51.5, 1, '111.3 km each way north/south; 69.3 km each way east/west'],
        [-60, 1, '111.3 km each way north/south; 55.7 km each way east/west'],
        [90, 1, '111.3 km each way north/south; 0.0 km each way east/west'],
        [0, 0.5, '55.7 km each way north/south; 55.7 km each way east/west']
    ]) {
        assert.equal(createPage(latitude, radius).distance.textContent, 'Approx. reach from centre: ' + expected + '.');
    }
    const page = createPage(0, 1);
    page.latitudeInput.valueAsNumber = 60;
    page.latitudeInput.input();
    assert.match(page.distance.textContent, /55.7 km each way east\/west/);
    page.radiusInput.valueAsNumber = 2;
    page.radiusInput.input();
    assert.match(page.distance.textContent, /222.6 km each way north\/south; 111.3 km each way east\/west/);
});

test('missing or invalid latitude and radius show guidance, not misleading distances', () => {
    for (const inputName of ['latitudeInput', 'radiusInput']) {
        const page = createPage();
        page[inputName].valueAsNumber = NaN;
        page[inputName].input();
        assert.equal(page.distance.textContent, 'Enter a valid latitude and radius to estimate distances.');
        page[inputName].valueAsNumber = 1;
        page[inputName].validity.valid = false;
        page[inputName].input();
        assert.equal(page.distance.textContent, 'Enter a valid latitude and radius to estimate distances.');
    }
});
