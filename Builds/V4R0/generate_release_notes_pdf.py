from reportlab.lib.pagesizes import letter
from reportlab.pdfgen import canvas
from reportlab.lib import colors
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, PageBreak
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.units import inch
import os

def md_to_pdf(md_path, pdf_path):
    with open(md_path, 'r', encoding='utf-8') as f:
        content = f.read()

    doc = SimpleDocTemplate(pdf_path, pagesize=letter,
                           rightMargin=50, leftMargin=50,
                           topMargin=50, bottomMargin=50)
    
    styles = getSampleStyleSheet()
    
    title_style = ParagraphStyle(
        'CustomTitle',
        parent=styles['Heading1'],
        fontSize=24,
        textColor=colors.HexColor('#003366'),
        spaceAfter=6,
        alignment=1
    )
    
    heading_style = ParagraphStyle(
        'CustomHeading',
        parent=styles['Heading2'],
        fontSize=14,
        textColor=colors.HexColor('#003366'),
        spaceAfter=12,
        spaceBefore=12
    )
    
    normal_style = ParagraphStyle(
        'CustomNormal',
        parent=styles['Normal'],
        fontSize=11,
        spaceAfter=6,
        leading=14
    )
    
    bullet_style = ParagraphStyle(
        'BulletStyle',
        parent=styles['Normal'],
        fontSize=11,
        leftIndent=20,
        spaceAfter=6,
        leading=14
    )
    
    elements = []
    lines = content.split('\n')
    
    for line in lines:
        line = line.rstrip()
        
        if line.startswith('# '):
            title = line.replace('# ', '')
            elements.append(Paragraph(title, title_style))
        elif line.startswith('## '):
            heading = line.replace('## ', '')
            elements.append(Paragraph(heading, heading_style))
        elif line.startswith('**') and line.endswith('**'):
            bold_text = line.replace('**', '')
            elements.append(Paragraph(f'<b>{bold_text}</b>', normal_style))
        elif line.startswith('- '):
            bullet = line[2:]
            elements.append(Paragraph(f'• {bullet}', bullet_style))
        elif line.strip() == '':
            elements.append(Spacer(1, 0.1*inch))
        else:
            elements.append(Paragraph(line, normal_style))
    
    doc.build(elements)

if __name__ == '__main__':
    base = os.path.dirname(__file__)
    md = os.path.join(base, 'RELEASE_NOTES_V4R0.md')
    pdf = os.path.join(base, 'RELEASE_NOTES_V4R0.pdf')
    md_to_pdf(md, pdf)
